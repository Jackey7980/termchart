// router.cpp —— 六条管道路由实现
//
// 每个管道做三件事：取列 → 聚合成"干净数据" → 用闭包捕获干净数据产出候选。
// 聚合全部走 autoviz 内部的类别编码/分桶/去重助手（data::group_reduce 的键
// 只能是 double，字符串维度在这里编码成 id）。
//
// 候选名 = chart_name::kXxx（与 chart/ 下的类一一对应）。扩展新图 = 在对应
// 管道里加一个候选 + semantic/score 里补规则，核心管线不动（规格书可扩展性）。
#include "termchart/autoviz/router.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "termchart/autoviz/domain.hpp"
#include "termchart/autoviz/time_parse.hpp"
#include "termchart/chart/cartesian/bar_chart.hpp"
#include "termchart/chart/cartesian/boxplot.hpp"
#include "termchart/chart/cartesian/bubble.hpp"
#include "termchart/chart/cartesian/heatmap.hpp"
#include "termchart/chart/cartesian/hist.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/chart/cartesian/parallel.hpp"
#include "termchart/chart/cartesian/scatter_chart.hpp"
#include "termchart/chart/geo/hexbin.hpp"
#include "termchart/chart/polar/pie.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/chart/statistical/corr.hpp"
#include "termchart/chart/text/table.hpp"
#include "termchart/chart/text/wordcloud.hpp"
#include "termchart/data/aggregate.hpp"

namespace termchart::autoviz {
namespace {

using termchart::chart::BarChart;
using termchart::chart::BoxPlotChart;
using termchart::chart::BubbleChart;
using termchart::chart::Chart;
using termchart::chart::CorrMatrixChart;
using termchart::chart::DefaultColor;
using termchart::chart::HeatmapChart;
using termchart::chart::HistChart;
using termchart::chart::Hexbin;
using termchart::chart::LineChart;
using termchart::chart::ParallelChart;
using termchart::chart::PieChart;
using termchart::chart::ScatterChart;
using termchart::chart::Series;
using termchart::chart::Table;
using termchart::chart::Wordcloud;
using termchart::chart::default_color;
using termchart::data::DataFrame;

// ── 列访问助手 ──────────────────────────────────────────────────────────────

// 轴域留痕（标题后缀）：裁了要说"裁去 N 个"，检测到离群却放弃裁剪更要说——
// 小样本保护闸退回原始域后，图顶端那片贴边平台会重新出现，读者若不知道
// 那是个 1e21，就会把它当成真实的业务高峰。两种情形都不许静默。
// 返回**不带前导分隔符**的正文（例如 "裁去 2 个极值"）：图题拼 " · " + 它，
// ChartCandidate::final_clip_desc 直接存它。两者必须同源——各写各的必然打架，
// 这正是"图题说裁了 2 个、告警说不裁轴"的旧病根。
std::string clip_desc(const RobustDomain& d) {
    if (d.active()) {
        return "裁去 " + std::to_string(d.clipped) + " 个极值";
    }
    if (d.abandoned()) {
        const char* why = d.decision == ClipDecision::kSmallSample ? "有效值太少"
                                                                  : "越界占比过高";
        return std::to_string(d.clipped) + " 个极值未裁（" + why + "）";
    }
    return {};
}

// 图题后缀：有结论才加分隔符。
std::string with_clip_note(std::string_view title, std::string_view desc) {
    return desc.empty() ? std::string(title) : std::string(title) + " · " + std::string(desc);
}

// 取一列的数值样本（非缺失且有限；NaN 单元格跳过）。
std::vector<double> numeric_samples(const DataFrame& frame, std::size_t column) {
    std::vector<double> out;
    if (column >= frame.columns.size()) {
        return out;
    }
    for (const data::Cell& cell : frame.columns[column]) {
        if (cell.is_numeric() && std::isfinite(cell.num)) {
            out.push_back(cell.num);
        }
    }
    return out;
}

// 类别编码：字符串 → 稠密 id（按**字典序**，保证输出确定性——同输入同候选）。
// 用 std::less<> 的 map 而非 unordered_map：前者支持 string_view 透明查找
// （unordered_map 的异构查找要自定义 hash+equal，不值当）。
class CategoryEncoder {
public:
    std::size_t encode(std::string_view key) {
        const auto it = index_.find(key);
        if (it != index_.end()) {
            return it->second;
        }
        const std::string owned(key);
        order_.push_back(owned);
        const std::size_t id = order_.size() - 1;
        index_.emplace(owned, id);
        return id;
    }
    void sort_by_key() {  // 字典序重排（order_ 与 id 映射一起改）
        std::vector<std::string> sorted = order_;
        std::sort(sorted.begin(), sorted.end());
        std::map<std::string, std::size_t, std::less<>> remap;
        for (std::size_t i = 0; i < sorted.size(); ++i) {
            remap[sorted[i]] = i;
        }
        for (std::size_t i = 0; i < order_.size(); ++i) {
            index_[order_[i]] = remap[order_[i]];
        }
        order_ = std::move(sorted);
    }
    [[nodiscard]] const std::vector<std::string>& order() const noexcept { return order_; }
    [[nodiscard]] std::size_t size() const noexcept { return order_.size(); }

private:
    std::vector<std::string> order_;
    std::map<std::string, std::size_t, std::less<>> index_;
};

// 行级三元组：(分类 id, 值) —— 事件/分类管道的聚合输入。任一侧缺失的行跳过。
struct CategoryValue {
    std::size_t category;
    double value;
};

std::vector<CategoryValue> category_values(const DataFrame& frame, std::size_t category_column,
                                           std::size_t value_column) {
    std::vector<CategoryValue> out;
    if (category_column >= frame.columns.size() || value_column >= frame.columns.size()) {
        return out;
    }
    const std::vector<data::Cell>& keys = frame.columns[category_column];
    const std::vector<data::Cell>& values = frame.columns[value_column];
    const std::size_t rows = std::min(keys.size(), values.size());
    CategoryEncoder encoder;
    for (std::size_t r = 0; r < rows; ++r) {
        if (keys[r].is_missing() || !values[r].is_numeric() || !std::isfinite(values[r].num)) {
            continue;
        }
        out.push_back(CategoryValue{encoder.encode(keys[r].raw), values[r].num});
    }
    // 重映射成字典序 id（上面的 id 是首次出现序，对输出不友好）。
    encoder.sort_by_key();
    const std::vector<std::string>& order = encoder.order();
    std::unordered_map<std::string, std::size_t> sorted_id;
    for (std::size_t i = 0; i < order.size(); ++i) {
        sorted_id[order[i]] = i;
    }
    // 重新按字典序 id 输出（保持每行原值）。
    std::vector<CategoryValue> remapped;
    remapped.reserve(out.size());
    for (const CategoryValue& cv : out) {
        remapped.push_back(CategoryValue{sorted_id[order[cv.category]], cv.value});
    }
    return remapped;
}

// 每个类别的均值/和（类别数 = categories.size()；空类为 0）。
std::vector<double> reduce_by_category(const std::vector<CategoryValue>& rows,
                                       std::size_t categories, bool sum) {
    std::vector<double> acc(categories, 0.0);
    std::vector<std::size_t> count(categories, 0);
    for (const CategoryValue& cv : rows) {
        if (cv.category >= categories) {
            continue;
        }
        acc[cv.category] += cv.value;
        ++count[cv.category];
    }
    if (!sum) {
        for (std::size_t i = 0; i < categories; ++i) {
            acc[i] = count[i] > 0 ? acc[i] / static_cast<double>(count[i]) : 0.0;
        }
    }
    return acc;
}

// 分类名列表（字典序）——category_values 已按字典序编码，直接取。
std::vector<std::string> sorted_category_names(const DataFrame& frame, std::size_t column) {
    std::vector<std::string> names;
    if (column >= frame.columns.size()) {
        return names;
    }
    for (const data::Cell& cell : frame.columns[column]) {
        if (!cell.is_missing()) {
            names.push_back(cell.raw);
        }
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

// 系列配色轮换（chart/series.hpp 的唯一真源默认色，按排名取档）。
// 色板容量 = 6：超过它的分类**不强行轮换**——第 7/8 类与第 1/2 类同色
// 比不分色更误导（用户会以为同色即同类）。管道侧据此把颜色通道降级为
// 单系列（见 route_geo / route_category_numeric 的 color_split）。
constexpr std::size_t kColorCycleSize = 6;

Color series_color(std::size_t rank) {
    static constexpr DefaultColor kCycle[] = {DefaultColor::Accent, DefaultColor::Neutral,
                                              DefaultColor::Cyan,   DefaultColor::Amber,
                                              DefaultColor::Warn,   DefaultColor::Muted};
    return default_color(kCycle[rank % kColorCycleSize]);
}
static_assert(kColorCycleSize == 6, "kCycle 与 kColorCycleSize 必须同步");

// ── 管道一：时间 ────────────────────────────────────────────────────────────

void route_time(const DataFrame& frame, const DatasetFeatures& f, const AutoVizOptions& options,
                std::vector<ChartCandidate>& out) {
    if (f.time_columns.empty() || f.numeric_columns.empty()) {
        return;
    }
    const std::size_t time_column = f.time_columns.front();
    const std::vector<data::Cell>& times = frame.columns[time_column];

    // 收集 (epoch, 值...) 与可选分类 id。
    std::vector<double> epochs;
    std::vector<std::vector<double>> values(f.numeric_columns.size());
    CategoryEncoder category;
    std::vector<std::size_t> category_ids;
    const bool with_category = f.has_category();
    const std::size_t category_column = with_category ? f.category_columns.front() : 0;
    std::size_t dropped_percent = 0;  // 被量纲闸拦下的百分比格数（只用于说明）
    // 源数据本身的缺失计数（与量纲闸分开：闸门拦下的格**存在**，只是量纲不同）。
    std::vector<std::size_t> source_missing(f.numeric_columns.size(), 0);

    // 时间列解析失败的行会被跳过——必须留痕，否则读者看到的是"凭空少的点"，
    // 而不知道是 2026/13/40 这种非法日期、还是 1740787260 这种 epoch 数字串
    // （后者刻意不认：与普通大整数不可区分，硬解释会把 1970/2025 塞进 2026 的轴）。
    std::size_t dropped_time = 0;
    std::vector<std::string> dropped_time_samples;

    for (std::size_t r = 0; r < frame.row_count && r < times.size(); ++r) {
        const std::optional<double> t = parse_timestamp(times[r].raw);
        if (!t.has_value()) {
            if (!times[r].raw.empty()) {
                ++dropped_time;
                if (dropped_time_samples.size() < 2) {
                    dropped_time_samples.push_back(times[r].raw);
                }
            }
            continue;
        }
        epochs.push_back(*t);
        for (std::size_t c = 0; c < f.numeric_columns.size(); ++c) {
            const std::size_t col = f.numeric_columns[c];
            const std::vector<data::Cell>& column = frame.columns[col];
            const bool in_range = r < column.size();
            const bool source_ok =
                in_range && column[r].is_numeric() && std::isfinite(column[r].num);
            // 量纲闸：百分比与纯金额混排时，百分点（0.15）不能和金额（1234.5）一起
            // 求均值——两者差 4 个数量级，几个百分点就能把整段均值拽成 0，读者会
            // 以为"这段时间没交易"。百分比格按缺失处理（点断掉），并记下次数，
            // **绝不乘 100 去凑**：那是把 15% 伪造成 15 元，属于编数据。
            const bool gated =
                source_ok && f.profiles[col].mixed_units &&
                data::has_cell_flag(column[r].flags, data::CellFlag::Percent);
            if (gated) {
                ++dropped_percent;
            }
            if (!source_ok) {
                ++source_missing[c];
            }
            values[c].push_back(gated || !source_ok ? std::nan("") : column[r].num);
        }
        if (with_category) {
            const std::vector<data::Cell>& keys = frame.columns[category_column];
            category_ids.push_back(r < keys.size() && !keys[r].is_missing()
                                       ? category.encode(keys[r].raw)
                                       : static_cast<std::size_t>(-1));
        }
    }
    if (epochs.size() < 2) {
        return;  // 时间管道至少两个时间点，否则折线/热力图都无意义
    }
    const auto [tmin, tmax] = std::minmax_element(epochs.begin(), epochs.end());
    const double width =
        nice_bucket_seconds(*tmin, *tmax, std::max<std::size_t>(4, options.canvas_width / 3));
    const std::size_t buckets =
        static_cast<std::size_t>((*tmax - *tmin) / width) + 1;
    const std::size_t series_cap = std::min<std::size_t>(values.size(), 5);
    const std::string value_label =
        series_cap > 1 ? f.profiles[f.numeric_columns[0]].name + " 等 " +
                             std::to_string(series_cap) + " 列"
                       : f.profiles[f.numeric_columns[0]].name;
    // 桶宽说人话：7200s 读不出来，"2 小时" 一眼就懂。
    const std::string line_title =
        value_label + " 随时间（按 " + format_span_human(width) + "均值）";

    // 稳健 Y 域：一列里混进 1e21 这种脏值时，[min, max] 会把其余数据全压成
    // 贴地的一条平线——图还在画、信息量归零。用 Tukey 篱裁出"主体范围"，
    // 篱外的点交给比例尺钳到轴边缘（保持时间序列连续，不用断线骗人）。
    // 多系列取域并集：共享一根 Y 轴就不能各裁一套，否则读者会以为它们同量级。
    RobustDomain line_domain;
    if (options.clip_outliers) {
        for (std::size_t s = 0; s < series_cap; ++s) {
            line_domain = merge_domains(line_domain,
                                        robust_y_domain(values[s], options.outlier_fence));
        }
    }
    // 标题留痕：轴被裁过就必须说，否则读者会把"顶端那个平台"当成真值；
    // 检测到离群但小样本保护闸放弃裁剪时同样要说（"未裁"）。
    const std::string line_clip_desc = clip_desc(line_domain);
    std::string line_title_note = with_clip_note(line_title, line_clip_desc);

    // 候选 1：折线（每数值列一条系列，桶内取均值）。
    {
        ChartCandidate c;
        c.name = chart_name::kLine;
        c.pipeline = PipelineKind::Time;
        c.title = line_title_note;
        c.clipped_points = line_domain.active() ? line_domain.clipped : 0;
        c.final_clip_desc = line_clip_desc;
        if (dropped_percent > 0) {
            c.notes.push_back("有 " + std::to_string(dropped_percent) +
                              " 个百分比单元格与纯数值同列（量纲不一致），已按缺失处理，"
                              "未做任何换算");
        }
        if (dropped_time > 0) {
            std::string note = "有 " + std::to_string(dropped_time) + " 行时间无法解析（";
            for (std::size_t i = 0; i < dropped_time_samples.size(); ++i) {
                if (i > 0) {
                    note += "、";
                }
                std::string raw = dropped_time_samples[i];
                if (raw.size() > 20) {
                    raw = raw.substr(0, 20) + "…";
                }
                note += "\"" + raw + "\"";
            }
            note += "），已跳过；X 轴只覆盖可解析的时间段";
            c.notes.push_back(std::move(note));
        }
        c.x_is_temporal = true;
        c.temporal_sorted = true;  // 桶序即时间序，聚合后天然有序
        c.point_count = buckets;
        c.series_count = series_cap;
        const std::size_t ncols = series_cap;
        // 每系列收集 (桶号, 值)，建图时按桶聚合均值——同桶多点不能直接串成折线
        // （行序未必按时间，会画成乱穿的线）。
        std::vector<std::vector<std::pair<std::size_t, double>>> samples(ncols);
        for (std::size_t r = 0; r < epochs.size(); ++r) {
            const std::size_t b = static_cast<std::size_t>((epochs[r] - *tmin) / width);
            for (std::size_t s = 0; s < ncols; ++s) {
                if (std::isfinite(values[s][r])) {
                    samples[s].emplace_back(b, values[s][r]);
                }
            }
        }
        std::vector<std::string> names;
        for (std::size_t s = 0; s < ncols; ++s) {
            names.push_back(f.profiles[f.numeric_columns[s]].name);
        }
        // 空值惩罚只按**源数据缺失**算，不把量纲闸的拦截再罚一次：同一个格子
        // 被"点消失"和"空值扣分"两次记账，曾让一个 15% 把折线解析分从 0.968
        // 压到 0.918，输给事件柱状的 0.953——图种被一个单元格式改写，而用户
        // 得到的是一张与时间无关的柱状图。闸门的后果由 notes 如实说明。
        std::vector<double> nulls(ncols, 0.0);
        for (std::size_t s = 0; s < ncols; ++s) {
            nulls[s] = values[s].empty()
                           ? 0.0
                           : static_cast<double>(source_missing[s]) /
                                 static_cast<double>(values[s].size());
        }
        const double worst_null = *std::max_element(nulls.begin(), nulls.end());
        c.value_null_rate = worst_null;
        auto ss = samples;
        auto labels = names;
        auto t0 = *tmin;
        auto w = width;
        auto span = *tmax - *tmin;
        // x 轴刻度必须**按日历对齐**，不能交给 scale::ticks_for：后者在 epoch 秒上
        // 按 10 的幂取整，41 小时的序列会被切成 50000 秒（13 小时 53 分）一格，
        // 端点还落在 02-28 17:33:20 这种假精度时刻上（真 bug：图上出现过 02-28 17:33
        // ~ 03-03 01:06 的轴，而数据只覆盖 03-01 03:00 ~ 03-02 20:00）。
        // 先挑人类可读的跨度（1h/6h/1d…），再让它落在该跨度的整数倍时刻上。
        const double tick_step = nice_bucket_seconds(*tmin, *tmax, 5);
        std::vector<double> x_ticks;
        for (double v = std::floor(*tmin / tick_step) * tick_step; v <= *tmax;
             v += tick_step) {
            if (v >= *tmin) {
                x_ticks.push_back(v);
            }
        }
        if (x_ticks.size() < 2) {
            x_ticks.clear();  // 跨度过小取不出两个刻度：退回自动刻度，别只标一个点
        }
        // 相邻刻度的间距（决定标签粒度）；取不到显式刻度时沿用四分法。
        auto resolution = x_ticks.size() >= 2 ? x_ticks[1] - x_ticks[0] : span / 4.0;
        const std::optional<scale::Domain> ydom =
            line_domain.active()
                ? std::optional<scale::Domain>{scale::Domain{line_domain.lo, line_domain.hi}}
                : std::nullopt;
        c.build = [ss, labels, t0, w, span, resolution, line_title_note, ydom, x_ticks]() {
            auto chart = std::make_unique<LineChart>();
            for (std::size_t s = 0; s < ss.size(); ++s) {
                // 桶内聚合均值，输出按桶号升序（x 严格有序）。
                std::vector<std::pair<std::size_t, double>> sorted = ss[s];
                std::sort(sorted.begin(), sorted.end());
                Series series;
                series.name = labels[s];
                series.color = series_color(s);
                std::size_t i = 0;
                while (i < sorted.size()) {
                    std::size_t j = i;
                    double sum = 0.0;
                    while (j < sorted.size() && sorted[j].first == sorted[i].first) {
                        sum += sorted[j].second;
                        ++j;
                    }
                    const double mean = sum / static_cast<double>(j - i);
                    series.points.push_back(
                        Point{t0 + static_cast<double>(sorted[i].first) * w, mean});
                    i = j;
                }
                chart->add_series(std::move(series));
            }
            chart->set_title(line_title_note);
            if (ydom.has_value()) {
                chart->set_y_domain(ydom->min, ydom->max);
            }
            // x 是 epoch 秒：位置交给日历对齐的显式刻度，文字换成 "03-01"/"12:00"。
            chart->set_x_ticks(x_ticks);
            chart->set_x_tick_formatter([span, resolution](double v) {
                return format_timestamp_short(v, span, resolution);
            });
            return chart;
        };
        out.push_back(std::move(c));
    }

    // 候选 2：时间 × 分类 密度热力图（有分类列时）。
    if (with_category && category.size() >= 2) {
        ChartCandidate c;
        c.name = chart_name::kHeatmap;
        c.pipeline = PipelineKind::Time;
        c.title = "时间 × " + f.profiles[category_column].name + " 密度";
        c.point_count = epochs.size();
        c.category_count = category.size();
        c.value_null_rate = 0.0;
        std::vector<Point> points;
        points.reserve(epochs.size());
        for (std::size_t r = 0; r < epochs.size(); ++r) {
            const std::size_t b = static_cast<std::size_t>((epochs[r] - *tmin) / width);
            if (category_ids[r] == static_cast<std::size_t>(-1)) {
                continue;
            }
            points.push_back(Point{*tmin + static_cast<double>(b) * width,
                                   static_cast<double>(category_ids[r])});
        }
        auto pts = points;
        c.build = [pts]() {
            auto chart = std::make_unique<HeatmapChart>();
            chart->set_values(pts);
            chart->set_title("时间密度");
            return chart;
        };
        out.push_back(std::move(c));
    }

    // 候选 3：每时间桶事件数柱状图（有分类列时）。
    if (with_category) {
        std::vector<double> counts(buckets, 0.0);
        for (const double t : epochs) {
            const std::size_t b = static_cast<std::size_t>((t - *tmin) / width);
            if (b < buckets) {
                ++counts[b];
            }
        }
        std::vector<std::string> labels;
        labels.reserve(buckets);
        for (std::size_t b = 0; b < buckets; ++b) {
            labels.push_back(std::to_string(b));
        }
        ChartCandidate c;
        c.name = chart_name::kBar;
        c.pipeline = PipelineKind::Time;
        c.title = "每时间桶事件数";
        c.point_count = buckets;
        c.category_count = buckets;
        c.x_is_temporal = true;
        c.temporal_sorted = true;
        auto cs = counts;
        auto ls = labels;
        c.build = [cs, ls]() {
            auto chart = std::make_unique<BarChart>();
            chart->set_values(cs, ls);
            chart->set_title("每时间桶事件数");
            return chart;
        };
        out.push_back(std::move(c));
    }
}

// ── 管道二：地理 ────────────────────────────────────────────────────────────

void route_geo(const DataFrame& frame, const DatasetFeatures& f, const AutoVizOptions& options,
               std::vector<ChartCandidate>& out) {
    if (f.geo_pairs.empty()) {
        return;
    }
    const GeoPair pair = f.geo_pairs.front();
    const std::vector<data::Cell>& lats = frame.columns[pair.lat];
    const std::vector<data::Cell>& lngs = frame.columns[pair.lng];
    const std::size_t rows = std::min(lats.size(), lngs.size());

    // 实体键：有 Low 档分类列时 = 分类 + 坐标（保留同实体多次上报的最后一条）；
    // 否则 = 坐标本身（四舍五入到 3 位小数去重）。**地理管道强制去重**——未去重
    // 的地理散点会把"上报次数"画成"地点数量"（规格书否决项，这里从源头保证）。
    const bool with_entity = f.has_category();
    const std::size_t entity_column = with_entity ? f.category_columns.front() : 0;
    struct GeoPoint {
        double lat;
        double lng;
        std::size_t entity;
        double value;
    };
    std::unordered_map<std::string, GeoPoint> dedup;
    CategoryEncoder entity_encoder;
    std::size_t max_entities = 0;
    std::vector<double> extra_values;  // 第二数值列（气泡大小用）
    const bool has_extra = f.numeric_columns.size() >= 2;
    const std::size_t extra_column = has_extra ? f.numeric_columns[1] : 0;

    for (std::size_t r = 0; r < rows; ++r) {
        if (!lats[r].is_numeric() || !lngs[r].is_numeric() || !std::isfinite(lats[r].num) ||
            !std::isfinite(lngs[r].num)) {
            continue;
        }
        const double lat_r = std::round(lats[r].num * 1000.0) / 1000.0;
        const double lng_r = std::round(lngs[r].num * 1000.0) / 1000.0;
        std::string key = std::to_string(lat_r) + "," + std::to_string(lng_r);
        std::size_t entity = 0;
        if (with_entity) {
            const std::vector<data::Cell>& keys = frame.columns[entity_column];
            if (r < keys.size() && !keys[r].is_missing()) {
                entity = entity_encoder.encode(keys[r].raw);
                key = std::to_string(entity) + "|" + key;
            }
        }
        double value = 1.0;
        if (!f.numeric_columns.empty()) {
            const std::vector<data::Cell>& vcol = frame.columns[f.numeric_columns.front()];
            if (r < vcol.size() && vcol[r].is_numeric() && std::isfinite(vcol[r].num)) {
                value = vcol[r].num;
            }
        }
        dedup[key] = GeoPoint{lat_r, lng_r, entity, value};  // 后写覆盖（保留最新）
    }
    max_entities = entity_encoder.size();
    if (dedup.empty()) {
        return;
    }
    std::vector<GeoPoint> points;
    points.reserve(dedup.size());
    for (const auto& [key, point] : dedup) {
        points.push_back(point);
    }
    std::vector<std::string> entity_names = entity_encoder.order();
    std::sort(entity_names.begin(), entity_names.end());

    // 候选 1：地理散点（按实体分系列；实体数超过色板容量降级为单系列——
    // 同色不同类比分不出颜色更糟）。
    {
        const bool split_series =
            with_entity && max_entities >= 2 && max_entities <= kColorCycleSize;
        ChartCandidate c;
        c.name = chart_name::kScatter;
        c.pipeline = PipelineKind::Geo;
        c.title = "地理分布（已去重）";
        c.point_count = points.size();
        c.series_count = split_series ? max_entities : 1;
        c.category_count = max_entities;
        c.geo_deduped = true;
        auto pts = points;
        auto names = entity_names;
        auto split = split_series;
        c.build = [pts, names, split]() {
            auto chart = std::make_unique<ScatterChart>();
            if (split) {
                for (std::size_t e = 0; e < names.size(); ++e) {
                    Series series;
                    series.name = names[e];
                    series.color = series_color(e);
                    for (const GeoPoint& p : pts) {
                        if (p.entity == e) {
                            series.points.push_back(Point{p.lng, p.lat});
                        }
                    }
                    if (!series.points.empty()) {
                        chart->add_series(std::move(series));
                    }
                }
            } else {
                Series series;
                series.name = "location";
                series.color = series_color(0);
                for (const GeoPoint& p : pts) {
                    series.points.push_back(Point{p.lng, p.lat});
                }
                chart->add_series(std::move(series));
            }
            chart->set_title("地理分布");
            return chart;
        };
        out.push_back(std::move(c));
    }

    // 候选 2：Hexbin 密度（点多于 100 时才值得）。
    if (points.size() > 100) {
        ChartCandidate c;
        c.name = chart_name::kHexbin;
        c.pipeline = PipelineKind::Geo;
        c.title = "地理密度（Hexbin）";
        c.point_count = points.size();
        c.geo_deduped = true;
        auto pts = points;
        c.build = [pts]() {
            auto chart = std::make_unique<Hexbin>();
            for (const GeoPoint& p : pts) {
                chart->add_point(p.lng, p.lat);
            }
            chart->set_title("地理密度");
            return chart;
        };
        out.push_back(std::move(c));
    }

    // 候选 3：气泡（有第二数值列时，大小 = 该列）。
    if (has_extra) {
        std::vector<double> radii;
        radii.reserve(points.size());
        const std::vector<data::Cell>& vcol = frame.columns[extra_column];
        for (std::size_t r = 0; r < rows; ++r) {
            if (r < vcol.size() && vcol[r].is_numeric() && std::isfinite(vcol[r].num)) {
                radii.push_back(vcol[r].num);
            }
        }
        ChartCandidate c;
        c.name = chart_name::kBubble;
        c.pipeline = PipelineKind::Geo;
        c.title = "地理气泡（大小 = " + f.profiles[extra_column].name + "）";
        c.point_count = points.size();
        c.geo_deduped = true;
        auto pts = points;
        auto rs = radii;
        c.build = [pts, rs]() {
            auto chart = std::make_unique<BubbleChart>();
            std::vector<BubbleChart::Bubble> bubbles;
            for (std::size_t i = 0; i < pts.size(); ++i) {
                BubbleChart::Bubble b;
                b.p = Point{pts[i].lng, pts[i].lat};
                b.r = i < rs.size() ? rs[i] : 0.0;
                bubbles.push_back(b);
            }
            chart->set_data(std::move(bubbles));
            chart->set_title("地理气泡");
            return chart;
        };
        out.push_back(std::move(c));
    }
}

// ── 管道三：事件/分类 ───────────────────────────────────────────────────────

void route_event(const DataFrame& frame, const DatasetFeatures& f, const AutoVizOptions& options,
                 std::vector<ChartCandidate>& out) {
    if (!f.has_category() || !f.has_numeric()) {
        return;
    }
    // event 列 = 强制指定（已在 features 置顶）或首个分类列；值列取前 3 个数值列。
    const std::size_t event_column = f.category_columns.front();
    const std::string event_name = f.profiles[event_column].name;
    const std::size_t value_cap = std::min<std::size_t>(f.numeric_columns.size(), 3);

    for (std::size_t v = 0; v < value_cap; ++v) {
        const std::size_t value_column = f.numeric_columns[v];
        const std::string value_name = f.profiles[value_column].name;
        std::vector<CategoryValue> rows = category_values(frame, event_column, value_column);
        if (rows.empty()) {
            continue;
        }
        const std::vector<std::string> categories = sorted_category_names(frame, event_column);
        // 分桶空值率：取信息量最大的桶（规格书"先分桶再查空值率"的落点）。
        const std::vector<data::BucketNullRate> buckets =
            data::null_rate_by_bucket(frame, event_name, value_name);
        double best_null = 1.0;
        for (const data::BucketNullRate& b : buckets) {
            if (b.rows >= 2) {
                best_null = std::min(best_null, b.null_rate);
            }
        }
        if (buckets.empty()) {
            best_null = f.profiles[value_column].null_rate;
        }
        bool any_negative = false;
        for (const CategoryValue& cv : rows) {
            if (cv.value < 0.0) {
                any_negative = true;
            }
        }

        // 候选 1：各 event 均值柱状图。
        {
            const std::vector<double> means = reduce_by_category(rows, categories.size(), false);
            // 稳健 Y 域：某一类的均值是 1e21 这种脏值时，别的类全被压平。
            // 柱的基线是 0 —— 域必须含 0，否则就是"截断轴"的谎（柱从轴底长）。
            RobustDomain dom;
            if (options.clip_outliers) {
                dom = robust_y_domain(means, options.outlier_fence);
            }
            // 先定"裁不裁"再动域：把 0 并进域里会改变 active() 的判据（并 0 后
            // 域可能不再比原始范围紧），不能一边改域一边问"裁了吗"。
            const bool bar_clipped = dom.active();
            const std::string bar_note = clip_desc(dom);
            if (bar_clipped) {
                dom.lo = std::min(dom.lo, 0.0);
                dom.hi = std::max(dom.hi, 0.0);
            }
            std::string bar_title = with_clip_note(
                "各 " + event_name + " 的 " + value_name + " 均值", bar_note);
            ChartCandidate c;
            c.name = chart_name::kBar;
            c.pipeline = PipelineKind::Event;
            c.title = bar_title;
            c.clipped_points = bar_clipped ? dom.clipped : 0;
            c.final_clip_desc = bar_note;
            c.point_count = categories.size();
            c.category_count = categories.size();
            c.x_is_categorical = true;
            c.value_null_rate = best_null;
            auto cs = categories;
            auto ms = means;
            auto vn = value_name;
            const std::optional<scale::Domain> ydom =
                bar_clipped && dom.usable()
                    ? std::optional<scale::Domain>{scale::Domain{dom.lo, dom.hi}}
                    : std::nullopt;
            c.build = [cs, ms, vn, bar_title, ydom]() {
                auto chart = std::make_unique<BarChart>();
                chart->set_values(ms, cs, default_color(DefaultColor::Accent), vn);
                chart->set_title(bar_title);
                if (ydom.has_value()) {
                    chart->set_y_domain(ydom->min, ydom->max);
                }
                return chart;
            };
            out.push_back(std::move(c));
        }

        // 候选 2：各 event 分布箱线图（贴分类标签；空类在组与标签两侧同步消失）。
        {
            ChartCandidate c;
            c.name = chart_name::kBoxplot;
            c.pipeline = PipelineKind::Event;
            c.title = "各 " + event_name + " 的 " + value_name + " 分布";
            c.point_count = rows.size();
            c.category_count = categories.size();
            c.x_is_categorical = true;
            c.value_null_rate = best_null;
            auto cs = categories;
            c.build = [rows, cs]() {
                auto chart = std::make_unique<BoxPlotChart>();
                std::vector<std::vector<double>> group_values(cs.size());
                for (const CategoryValue& cv : rows) {
                    if (cv.category < cs.size()) {
                        group_values[cv.category].push_back(cv.value);
                    }
                }
                std::vector<BoxPlotChart::BoxStats> groups;
                std::vector<std::string> labels;
                for (std::size_t i = 0; i < cs.size(); ++i) {
                    if (group_values[i].empty()) {
                        continue;
                    }
                    const data::Quartiles q = data::quartiles(
                        data::DataView{group_values[i].data(), group_values[i].size()});
                    BoxPlotChart::BoxStats stats;
                    stats.lo = q.minimum;
                    stats.q1 = q.q1;
                    stats.med = q.median;
                    stats.q3 = q.q3;
                    stats.hi = q.maximum;
                    groups.push_back(stats);
                    labels.push_back(cs[i]);
                }
                chart->set_data(std::move(groups));
                chart->set_labels(std::move(labels));
                chart->set_title("分布");
                return chart;
            };
            out.push_back(std::move(c));
        }

        // 候选 3：event 占比饼图（≤8 片且无负值才有意义；否决规则在 semantic）。
        if (categories.size() >= 2 && categories.size() <= 8) {
            const std::vector<double> sums = reduce_by_category(rows, categories.size(), true);
            ChartCandidate c;
            c.name = chart_name::kPie;
            c.pipeline = PipelineKind::Event;
            c.title = event_name + " 占比（" + value_name + "）";
            c.point_count = categories.size();
            c.category_count = categories.size();
            c.value_null_rate = best_null;
            c.has_negative_values = any_negative;
            auto cs = categories;
            auto ss = sums;
            c.build = [cs, ss]() {
                auto chart = std::make_unique<PieChart>();
                chart->set_values(ss, cs);
                chart->set_title("占比");
                return chart;
            };
            out.push_back(std::move(c));
        }
    }
}

// ── 管道四：分类 + 数值组合 ─────────────────────────────────────────────────

void route_category_numeric(const DataFrame& frame, const DatasetFeatures& f,
                            const AutoVizOptions& options, std::vector<ChartCandidate>& out) {
    if (!f.has_category() || f.numeric_columns.size() < 2) {
        return;  // 组合管道至少两个数值列（坐标 + 大小/第二坐标）
    }
    const std::size_t category_column = f.category_columns.front();
    const std::string category_name = f.profiles[category_column].name;
    const std::vector<std::string> categories = sorted_category_names(frame, category_column);
    if (categories.size() < 2) {
        return;  // 单类没有"按类别着色"可言
    }
    // 颜色通道容量 = 色板 6 色。7..8 类仍出图，但降级为**单系列**（不带颜色
    // 通道）——强行轮换会让第 7/8 类与第 1/2 类同色，比分不出颜色更误导。
    const bool color_split = categories.size() <= kColorCycleSize;
    const std::size_t x_column = f.numeric_columns[0];
    const std::size_t y_column = f.numeric_columns[1];
    const std::string x_name = f.profiles[x_column].name;
    const std::string y_name = f.profiles[y_column].name;

    // 收集 (x, y, 分类 id, size)。
    struct Row {
        double x;
        double y;
        std::size_t category;
        double size;
    };
    std::vector<Row> rows;
    CategoryEncoder encoder;
    const std::vector<data::Cell>& xs = frame.columns[x_column];
    const std::vector<data::Cell>& ys = frame.columns[y_column];
    const std::vector<data::Cell>& keys = frame.columns[category_column];
    const bool has_size = f.numeric_columns.size() >= 3;
    const std::vector<data::Cell>* sizes =
        has_size ? &frame.columns[f.numeric_columns[2]] : nullptr;
    const std::size_t n = std::min(xs.size(), ys.size());
    for (std::size_t r = 0; r < n; ++r) {
        if (!xs[r].is_numeric() || !ys[r].is_numeric() || !std::isfinite(xs[r].num) ||
            !std::isfinite(ys[r].num) || keys[r].is_missing()) {
            continue;
        }
        double size = 0.0;
        if (sizes != nullptr && r < sizes->size() && (*sizes)[r].is_numeric() &&
            std::isfinite((*sizes)[r].num)) {
            size = (*sizes)[r].num;
        }
        rows.push_back(Row{xs[r].num, ys[r].num, encoder.encode(keys[r].raw), size});
    }
    if (rows.size() < 3) {
        return;
    }
    const std::string size_name =
        has_size ? f.profiles[f.numeric_columns[2]].name : std::string();

    // 候选 1：按分类着色的散点（类数超色板容量时降级为单系列）。
    {
        ChartCandidate c;
        c.name = chart_name::kScatter;
        c.pipeline = PipelineKind::CategoryNumeric;
        c.title = color_split ? x_name + " × " + y_name + "（按 " + category_name + " 着色）"
                              : x_name + " × " + y_name;
        c.point_count = rows.size();
        c.series_count = color_split ? categories.size() : 1;
        c.category_count = categories.size();
        c.color_is_categorical = color_split;
        c.color_tier = f.profiles[category_column].tier;
        c.value_null_rate = f.profiles[y_column].null_rate;
        auto rs = rows;
        auto cs = categories;
        auto split = color_split;
        c.build = [rs, cs, split]() {
            auto chart = std::make_unique<ScatterChart>();
            if (!split) {
                Series series;
                series.name = "points";
                series.color = series_color(0);
                for (const Row& row : rs) {
                    series.points.push_back(Point{row.x, row.y});
                }
                chart->add_series(std::move(series));
                return chart;
            }
            for (std::size_t i = 0; i < cs.size(); ++i) {
                Series series;
                series.name = cs[i];
                series.color = series_color(i);
                for (const Row& row : rs) {
                    if (row.category == i) {
                        series.points.push_back(Point{row.x, row.y});
                    }
                }
                if (!series.points.empty()) {
                    chart->add_series(std::move(series));
                }
            }
            chart->set_title("着色散点");
            return chart;
        };
        out.push_back(std::move(c));
    }

    // 候选 2：气泡（有第三数值列时）。
    if (has_size) {
        ChartCandidate c;
        c.name = chart_name::kBubble;
        c.pipeline = PipelineKind::CategoryNumeric;
        c.title = x_name + " × " + y_name + "（大小 = " + size_name + "）";
        c.point_count = rows.size();
        c.category_count = categories.size();
        c.color_is_categorical = true;
        c.color_tier = f.profiles[category_column].tier;
        c.value_null_rate = f.profiles[y_column].null_rate;
        auto rs = rows;
        c.build = [rs]() {
            auto chart = std::make_unique<BubbleChart>();
            std::vector<BubbleChart::Bubble> bubbles;
            for (const Row& row : rs) {
                BubbleChart::Bubble b;
                b.p = Point{row.x, row.y};
                b.r = row.size;
                bubbles.push_back(b);
            }
            chart->set_data(std::move(bubbles));
            chart->set_title("气泡");
            return chart;
        };
        out.push_back(std::move(c));
    }
}

// ── 管道五：纯数值 ──────────────────────────────────────────────────────────

void route_numeric(const DataFrame& frame, const DatasetFeatures& f,
                   const AutoVizOptions& options, std::vector<ChartCandidate>& out) {
    if (!f.has_numeric()) {
        return;
    }
    const std::size_t count = f.numeric_columns.size();

    // 1 列 → 直方图（+ 单组箱线做伴：分布两种读法）。
    if (count == 1) {
        const std::size_t column = f.numeric_columns.front();
        const std::string name = f.profiles[column].name;
        const std::vector<double> samples = numeric_samples(frame, column);
        bool constant = false;
        if (!samples.empty()) {
            const auto [lo, hi] = std::minmax_element(samples.begin(), samples.end());
            constant = *lo == *hi;
        }
        ChartCandidate c;
        c.name = chart_name::kHist;
        c.pipeline = PipelineKind::Numeric;
        c.title = name + " 分布";
        c.point_count = samples.size();
        c.value_null_rate = f.profiles[column].null_rate;
        c.value_constant = constant;
        auto ss = samples;
        c.build = [ss]() {
            auto chart = std::make_unique<HistChart>();
            chart->set_values(ss);
            chart->set_title("分布");
            return chart;
        };
        out.push_back(std::move(c));
    }

    // 2 列 → 散点（+ 回归读法交给 scatter，终端不堆拟合线）。
    if (count == 2) {
        const std::size_t x_column = f.numeric_columns[0];
        const std::size_t y_column = f.numeric_columns[1];
        const std::string x_name = f.profiles[x_column].name;
        const std::string y_name = f.profiles[y_column].name;
        const std::vector<data::Cell>& xs = frame.columns[x_column];
        const std::vector<data::Cell>& ys = frame.columns[y_column];
        std::vector<Point> points;
        const std::size_t n = std::min(xs.size(), ys.size());
        for (std::size_t r = 0; r < n; ++r) {
            if (xs[r].is_numeric() && ys[r].is_numeric() && std::isfinite(xs[r].num) &&
                std::isfinite(ys[r].num)) {
                points.push_back(Point{xs[r].num, ys[r].num});
            }
        }
        ChartCandidate c;
        c.name = chart_name::kScatter;
        c.pipeline = PipelineKind::Numeric;
        c.title = x_name + " × " + y_name;
        c.point_count = points.size();
        c.value_null_rate =
            0.5 * (f.profiles[x_column].null_rate + f.profiles[y_column].null_rate);
        auto ps = points;
        c.build = [ps]() {
            auto chart = std::make_unique<ScatterChart>();
            Series series;
            series.name = "points";
            series.points = std::move(ps);
            chart->add_series(std::move(series));
            chart->set_title("散点");
            return chart;
        };
        out.push_back(std::move(c));
    }

    // ≥3 列 → 平行坐标 + 相关矩阵。
    if (count >= 3) {
        const std::size_t cap = std::min<std::size_t>(count, 8);
        std::vector<std::string> names;
        std::vector<std::vector<double>> columns(cap);
        for (std::size_t i = 0; i < cap; ++i) {
            names.push_back(f.profiles[f.numeric_columns[i]].name);
            columns[i] = numeric_samples(frame, f.numeric_columns[i]);
        }
        // 观测 = 行（各列最短长度对齐）。
        std::size_t rows_n = columns.empty() ? 0 : columns.front().size();
        for (const std::vector<double>& column : columns) {
            rows_n = std::min(rows_n, column.size());
        }
        std::vector<std::vector<double>> observations;
        observations.reserve(rows_n);
        for (std::size_t r = 0; r < rows_n; ++r) {
            std::vector<double> row(cap);
            for (std::size_t i = 0; i < cap; ++i) {
                row[i] = columns[i][r];
            }
            observations.push_back(std::move(row));
        }
        double worst_null = 0.0;
        for (std::size_t i = 0; i < cap; ++i) {
            worst_null = std::max(worst_null, f.profiles[f.numeric_columns[i]].null_rate);
        }

        ChartCandidate parallel;
        parallel.name = chart_name::kParallel;
        parallel.pipeline = PipelineKind::Numeric;
        parallel.title = "多维平行坐标";
        parallel.point_count = observations.size();
        parallel.series_count = observations.size();
        parallel.axis_count = cap;
        parallel.category_count = cap;
        parallel.value_null_rate = worst_null;
        auto ns = names;
        auto obs = observations;
        parallel.build = [ns, obs]() {
            auto chart = std::make_unique<ParallelChart>();
            chart->set_axes(ns, obs);
            chart->set_title("平行坐标");
            return chart;
        };
        out.push_back(std::move(parallel));

        ChartCandidate corr;
        corr.name = chart_name::kCorr;
        corr.pipeline = PipelineKind::Numeric;
        corr.title = "相关矩阵";
        corr.point_count = cap * (cap - 1) / 2;
        corr.category_count = cap;
        corr.value_null_rate = worst_null;
        auto ns2 = names;
        auto cols2 = columns;
        corr.build = [ns2, cols2]() {
            auto chart = std::make_unique<CorrMatrixChart>();
            chart->set_matrix(ns2, cols2);
            chart->set_title("相关矩阵");
            return chart;
        };
        out.push_back(std::move(corr));
    }
}

// ── 管道六：纯文本 ──────────────────────────────────────────────────────────
//
// 注意调用时机分两种（由 route() 决定）：
//   * 有其它可绘列时：只用 High 档自由文本列（text_columns）——词云是补充；
//   * 全表只有字符串时（无时间/地理/数值）：text_columns + category_columns
//     全上——单文本列可能因 distinct 少被判 Low 分类，但它仍然是词云仅有的
//     原料（"全字符串 → WordCloud"是规格书的 Text 管道，不能被分档饿死）。
void route_text_columns(const DataFrame& frame, const std::vector<std::size_t>& columns,
                        const AutoVizOptions& options, std::vector<ChartCandidate>& out) {
    // 词云：前两列的词频（去重后 <3 个词会被 semantic 否决）。
    std::unordered_map<std::string, std::size_t> frequency;
    const std::size_t cap = std::min<std::size_t>(columns.size(), 2);
    for (std::size_t c = 0; c < cap; ++c) {
        if (columns[c] >= frame.columns.size()) {
            continue;
        }
        for (const data::Cell& cell : frame.columns[columns[c]]) {
            if (!cell.is_missing() && !cell.raw.empty()) {
                ++frequency[cell.raw];
            }
        }
    }
    if (frequency.empty()) {
        return;
    }
    std::vector<std::pair<std::string, std::size_t>> words(frequency.begin(), frequency.end());
    std::sort(words.begin(), words.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    ChartCandidate c;
    c.name = chart_name::kWordcloud;
    c.pipeline = PipelineKind::Text;
    c.title = "词云";
    c.point_count = words.size();
    c.category_count = words.size();
    c.value_null_rate = 0.0;
    auto ws = words;
    c.build = [ws]() {
        auto chart = std::make_unique<Wordcloud>();
        for (const auto& [text, count] : ws) {
            chart->add_word(text, static_cast<double>(count));
        }
        chart->set_title("词云");
        return chart;
    };
    out.push_back(std::move(c));
}

void route_text(const DataFrame& frame, const DatasetFeatures& f, const AutoVizOptions& options,
                std::vector<ChartCandidate>& out) {
    if (!f.has_text()) {
        return;
    }
    route_text_columns(frame, f.text_columns, options, out);
}

// ── 兜底：数据预览 ──────────────────────────────────────────────────────────

void route_preview(const DataFrame& frame, const DatasetFeatures& f,
                   const AutoVizOptions& options, std::vector<ChartCandidate>& out) {
    // 前 20 行 × 前 8 列。**不过滤 dropped**——预览是"你的数据长这样"的 dump，
    // 不是图；把常量/高空值率列藏起来会让用户以为数据丢了（曾有 bug：单值列
    // 被当 Constant 丢掉后预览表为空，"任何输入都有答案"破产）。
    std::vector<std::size_t> columns;
    for (std::size_t i = 0; i < f.profiles.size() && columns.size() < 8; ++i) {
        columns.push_back(i);
    }
    if (columns.empty()) {
        return;
    }
    const std::size_t rows = std::min<std::size_t>(frame.row_count, 20);
    std::vector<std::string> names;
    for (const std::size_t i : columns) {
        names.push_back(f.profiles[i].name);
    }
    std::vector<std::vector<std::string>> data;
    data.reserve(rows);
    for (std::size_t r = 0; r < rows; ++r) {
        std::vector<std::string> row;
        for (const std::size_t i : columns) {
            const std::vector<data::Cell>& column = frame.columns[i];
            row.push_back(r < column.size() ? column[r].raw : std::string());
        }
        data.push_back(std::move(row));
    }
    ChartCandidate c;
    c.name = chart_name::kTable;
    c.pipeline = PipelineKind::Preview;
    c.title = "数据预览";
    c.point_count = data.size();
    auto ns = names;
    auto ds = data;
    c.build = [ns, ds]() {
        auto chart = std::make_unique<Table>();
        for (const std::string& name : ns) {
            chart->add_column(name);
        }
        for (const std::vector<std::string>& row : ds) {
            chart->add_row(row);
        }
        chart->set_title("数据预览");
        return chart;
    };
    out.push_back(std::move(c));
}

}  // namespace

std::vector<ChartCandidate> route(const DataFrame& frame, const DatasetFeatures& features,
                                  const AutoVizOptions& options) {
    std::vector<ChartCandidate> out;
    if (frame.columns.empty() || frame.row_count < options.min_rows) {
        route_preview(frame, features, options, out);
        return out;
    }
    route_time(frame, features, options, out);
    route_geo(frame, features, options, out);
    route_event(frame, features, options, out);
    route_category_numeric(frame, features, options, out);
    route_numeric(frame, features, options, out);

    // 文本管道：全表只有字符串时回退到全部字符串列（见 route_text_columns 注释）。
    if (!features.has_time() && !features.has_geo() && !features.has_numeric()) {
        std::vector<std::size_t> all_text = features.text_columns;
        for (const std::size_t i : features.category_columns) {
            all_text.push_back(i);
        }
        route_text_columns(frame, all_text, options, out);
    } else {
        route_text(frame, features, options, out);
    }

    route_preview(frame, features, options, out);
    return out;
}

}  // namespace termchart::autoviz
