// hist.cpp —— 笛卡尔直方图实现（自动分箱 → 逐箱画柱）
#include "termchart/chart/cartesian/hist.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/legend.hpp"
#include "termchart/chart/scene_sink.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

// Sturges 规则：k = ceil(log2(n)) + 1。样本量每翻倍多加一箱，只依赖样本数、
// 不依赖分布形态，因而**确定**（同一输入永远得到同一分箱），是自动分箱里最稳妥
// 的默认，也是主流统计软件的选项之一。
int sturges_bins(std::size_t n) noexcept {
    if (n <= 1) {
        return 1;
    }
    return static_cast<int>(std::ceil(std::log2(static_cast<double>(n)))) + 1;
}

constexpr int kMinBins = 1;
constexpr int kMaxBins = 64;  // 上限：再多只会画出 1 格宽的柱，没有信息增量

}  // namespace

void HistChart::set_values(std::vector<double> values, Color color, std::string name) {
    values_ = std::move(values);
    color_ = color;
    name_ = std::move(name);
    rebin();
}

void HistChart::rebin() {
    edges_.clear();
    counts_.clear();

    // 只保留有限样本：NaN/Inf 会让极值、箱宽与落箱索引全部失真，直接剔除比
    // "处处防空"更省事，也顺带保证定义域端点有限。
    std::vector<double> samples;
    samples.reserve(values_.size());
    for (double v : values_) {
        if (std::isfinite(v)) {
            samples.push_back(v);
        }
    }
    if (samples.empty()) {
        return;  // 无有效样本：分箱留空，draw 直接返回
    }

    double lo = samples.front();
    double hi = samples.front();
    for (double v : samples) {
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    if (!(hi > lo)) {
        hi = lo + 1.0;  // 全部同值：给单位宽度，避免零宽除零
    }

    const int bins =
        std::clamp(bins_ > 0 ? bins_ : sturges_bins(samples.size()), kMinBins, kMaxBins);
    const auto bin_count = static_cast<std::size_t>(bins);
    const double width = (hi - lo) / static_cast<double>(bins);

    edges_.resize(bin_count + 1);
    for (std::size_t i = 0; i <= bin_count; ++i) {
        edges_[i] = lo + width * static_cast<double>(i);
    }

    counts_.assign(bin_count, 0);
    for (double v : samples) {
        // 用 floor 落箱；最大值恰好等于 hi 时会落到 bin_count，钳回末箱，
        // 使区间按右闭 (lo, hi] 处理——每个样本都必有一箱。
        const int idx =
            std::clamp(static_cast<int>(std::floor((v - lo) / width)), 0, bins - 1);
        ++counts_[static_cast<std::size_t>(idx)];
    }
}

void HistChart::draw(DrawContext ctx) {
    if (counts_.empty()) {
        return;  // 无有效样本：不绘制，也避免按箱划分时除零
    }

    std::size_t max_count = 0;
    for (std::size_t c : counts_) {
        max_count = std::max(max_count, c);
    }

    // 定义域精确取 [箱首, 箱尾] × [0, 最大频数]：不套 nice_domain 的 5% 留白，
    // 这样最高柱恰好触顶、箱边界与 x 轴严格对齐；"含零"由 ymin=0 直接保证。
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, edges_.front(), edges_.back(), 0.0,
                      static_cast<double>(max_count), false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());

    // 上下文开关照既有图种落地：默认全开，导出/GUI 侧可以只要数据不要轴。
    const Style axis_style = ctx.axis_style;
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "value", axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "count", axis_style);
    }

    const Style st = Style{}.set_fg(color_);
    const Rect inner = area_.inner_rect();
    const double y0 = area_.y_scale().forward(0.0);
    for (std::size_t i = 0; i < counts_.size(); ++i) {
        const double x0 = area_.x_scale().forward(edges_[i]);
        const double x1 = area_.x_scale().forward(edges_[i + 1]);
        const double y1 = area_.y_scale().forward(static_cast<double>(counts_[i]));
        // 直方图的柱按箱相邻排列（无间隙）——箱本身就是相邻区间。
        ctx.canvas.fill_rect(Rect{x0, std::min(y0, y1), x1 - x0, std::fabs(y1 - y0)},
                             kBarChar, st);
    }

    if (show_legend_ && ctx.draw_legend) {
        const std::vector<layout::LegendEntry> entries{layout::LegendEntry{name_, color_}};
        layout::draw_legend(ctx.canvas, entries, Rect{inner.x, 0.0, inner.w, 1.0},
                            layout::HAlign::Left);
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

bool HistChart::to_scene(scene::Scene& out, std::uint32_t width, std::uint32_t height) const {
    if (counts_.empty()) {
        return true;
    }
    std::size_t max_count = 0;
    for (std::size_t c : counts_) {
        max_count = std::max(max_count, c);
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(width, height, m, edges_.front(), edges_.back(), 0.0,
                     static_cast<double>(max_count), false, false);
    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());

    SceneSink sink(out);
    layout::draw_grid(sink, area_, xticks, yticks,
                      Style{}.set_fg(default_color(DefaultColor::Muted)));
    layout::draw_axis_x(sink, area_, xticks, "value", Style{});
    layout::draw_axis_y(sink, area_, yticks, "count", Style{});

    const Style st = Style{}.set_fg(color_);
    const Rect inner = area_.inner_rect();
    const double y0 = area_.y_scale().forward(0.0);
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);
    for (std::size_t i = 0; i + 1 < edges_.size(); ++i) {
        const double x0 = area_.x_scale().forward(edges_[i]);
        const double x1 = area_.x_scale().forward(edges_[i + 1]);
        const double y1 = area_.y_scale().forward(static_cast<double>(counts_[i]));
        b.fill(st.fg());
        b.rect(Rect{x0, std::min(y0, y1), x1 - x0, std::fabs(y1 - y0)});
    }

    if (show_legend_) {
        const std::vector<layout::LegendEntry> entries{layout::LegendEntry{name_, color_}};
        layout::draw_legend(sink, entries, Rect{inner.x, 0.0, inner.w, 1.0},
                            layout::HAlign::Left);
    }
    if (!title_.empty()) {
        layout::draw_title(sink, title_, inner);
    }
    b.end_group(0);
    return true;
}

}  // namespace termchart::chart
