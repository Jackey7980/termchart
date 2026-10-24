// autoviz/candidate.hpp —— 候选图：规格 + 惰性建图闭包 + 打分槽位（L21）
#ifndef TERMCHART_AUTOVIZ_CANDIDATE_HPP
#define TERMCHART_AUTOVIZ_CANDIDATE_HPP

#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "termchart/autoviz/options.hpp"
#include "termchart/chart/chart.hpp"
#include "termchart/data/column_profile.hpp"
#include "termchart/export.hpp"

namespace termchart::autoviz {

// 惰性建图：路由阶段把"聚合后的干净数据"捕获进闭包，评分/渲染时才真正构造
// Chart——沙盒评估不建图的候选零成本，通过校验的 Top-N 才建。
using ChartBuilder = std::function<std::unique_ptr<chart::Chart>()>;

// 图种标识（autoviz 表内 key，与 chart/ 下的类一一对应，便于诊断）。
// 不改 ChartRegistry 的注册语义——那里是插件作者的扩展点；这里是 AutoViz
// 自己的静态决策表（照 raster::pick_raster 的扁平范式）。
namespace chart_name {
inline constexpr const char* kLine = "line_chart";
inline constexpr const char* kBar = "bar_chart";
inline constexpr const char* kHist = "hist_chart";
inline constexpr const char* kBoxplot = "boxplot_chart";
inline constexpr const char* kScatter = "scatter_chart";
inline constexpr const char* kBubble = "bubble_chart";
inline constexpr const char* kPie = "pie_chart";
inline constexpr const char* kHeatmap = "heatmap_chart";
inline constexpr const char* kHexbin = "hexbin_chart";
inline constexpr const char* kWordcloud = "wordcloud_chart";
inline constexpr const char* kParallel = "parallel_chart";
inline constexpr const char* kCorr = "corr_matrix_chart";
inline constexpr const char* kTable = "table_chart";
}  // namespace chart_name

struct ChartCandidate {
    // ── 规格 ──────────────────────────────────────────────────────────────
    std::string name;  // chart_name::kXxx
    PipelineKind pipeline = PipelineKind::Preview;
    std::string title;
    ChartBuilder build;  // 空 = 无效候选（路由不应产出，出现即 bug）

    // ── 打分元数据（路由阶段填，不依赖建图）────────────────────────────────
    std::size_t point_count = 0;      // 图中标记数（点/柱/词/桶）
    std::size_t series_count = 1;     // 系列数（折线/散点）
    std::size_t category_count = 0;   // 分类数（x 轴类别/饼图分片/词云词数）
    double value_null_rate = 0.0;     // 值列空值率（有 event 列时为最优桶口径）
    bool x_is_temporal = false;
    bool x_is_categorical = false;
    bool color_is_categorical = false;
    data::CardinalityTier color_tier = data::CardinalityTier::Low;
    bool has_negative_values = false;  // 饼图否决用
    bool value_constant = false;       // 值列无差异（直方图/柱状无意义）
    bool temporal_sorted = true;       // 时间轴是否已保证有序（管道内聚合规）
    bool geo_deduped = true;           // 地理管道强制去重；false 触发否决
    std::size_t axis_count = 0;        // 平行坐标轴数

    // ── 打分结果 ──────────────────────────────────────────────────────────
    double analytic_score = 0.0;
    double raster_score = std::nan("");  // NaN = 未做画布级复核
    std::vector<std::string> vetoes;     // 非空 = 出局（每条是结构化理由）

    [[nodiscard]] bool viable() const noexcept {
        return vetoes.empty() && static_cast<bool>(build);
    }
    // 光栅复核的最大扣分幅度（乘性微调，见 total_score）。
    static constexpr double kRasterWeight = 0.15;

    // 综合分：**解析分占绝对主导，复核只做下调**（乘性，不是加权平均）。
    //   total = analytic × (1 - k + k × raster)
    // 语义：raster=1（形状健康）→ 原样保留；raster=0（空图/糊图）→ 打 85 折；
    //       未复核（NaN）→ 同样乘 1.0。
    // 为什么不做"加权平均"（旧的 0.65a+0.35r）：raster 与 analytic 不同量纲，
    // 平均会让"复核通过但形状分只有 0.5"的候选凭空掉分，等于惩罚被复核的人。
    // 为什么乘数上界是 1.0 而不是 1.2：奖励复核过的候选，等于把"是否复核"
    // 又偷偷塞进分数，同样扭曲排序——复核的职责是**发现问题**，不是发奖金。
    [[nodiscard]] double total_score() const noexcept {
        if (!viable()) {
            return -1.0;
        }
        if (std::isnan(raster_score)) {
            return analytic_score;
        }
        const double multiplier = (1.0 - kRasterWeight) + kRasterWeight * raster_score;
        return analytic_score * multiplier;
    }
};

}  // namespace termchart::autoviz

#endif  // TERMCHART_AUTOVIZ_CANDIDATE_HPP
