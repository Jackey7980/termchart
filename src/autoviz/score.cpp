// score.cpp —— 候选打分实现
#include "termchart/autoviz/score.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/context.hpp"

namespace termchart::autoviz {
namespace {

double clamp01(double v) noexcept { return std::max(0.0, std::min(1.0, v)); }

// 覆盖率满意区间：太稀 = 空图（信息不足），太满 = 糊（不可读）。
constexpr double kCoverageLo = 0.02;
constexpr double kCoverageHi = 0.35;
constexpr double kRowUsageLo = 0.35;
constexpr double kRowUsageHi = 0.95;

double ramp(double value, double lo, double hi) noexcept {
    if (value < lo) {
        return lo > 0.0 ? clamp01(value / lo) : 1.0;
    }
    if (value > hi) {
        return hi < 1.0 ? clamp01((1.0 - value) / (1.0 - hi)) : 1.0;
    }
    return 1.0;
}

// 管道 × 图种的基础适配分（"这张图配这份数据吗"）。
double base_fit(const ChartCandidate& c) noexcept {
    switch (c.pipeline) {
    case PipelineKind::Time:
        if (c.name == chart_name::kLine) return 1.0;
        if (c.name == chart_name::kHeatmap) return 0.75;
        return 0.6;  // bar
    case PipelineKind::Geo:
        if (c.name == chart_name::kScatter) return 1.0;
        if (c.name == chart_name::kHexbin) return 0.8;
        return 0.75;  // bubble
    case PipelineKind::Event:
        if (c.name == chart_name::kBar) return 0.95;
        if (c.name == chart_name::kBoxplot) return 0.85;
        return 0.7;  // pie
    case PipelineKind::CategoryNumeric:
        return c.name == chart_name::kBubble ? 0.9 : 0.85;
    case PipelineKind::Text:
        return 0.9;
    case PipelineKind::Numeric:
        if (c.name == chart_name::kHist) return 0.95;
        if (c.name == chart_name::kScatter) return 0.9;
        if (c.name == chart_name::kParallel) return 0.8;
        return 0.75;  // corr
    case PipelineKind::Preview:
        return 0.1;
    }
    return 0.5;
}

}  // namespace

double analytic_score(const ChartCandidate& c) noexcept {
    if (!c.viable()) {
        return 0.0;
    }
    double s = base_fit(c);
    s += 0.05 * std::min(1.0, static_cast<double>(c.point_count) / 50.0);  // 数据量足
    s -= 0.5 * c.value_null_rate;                                          // 空值惩罚
    if (c.category_count > 12) {
        s -= 0.3;  // 类别过多：x 轴/图例必挤爆
    }
    if (c.series_count > 1) {
        s += 0.1;  // 多系列信息量大
    }
    return clamp01(s);
}

RasterStats raster_stats(const ChartCandidate& c, std::uint32_t width, std::uint32_t height) {
    RasterStats stats;
    if (!c.build || width == 0 || height == 0) {
        return stats;
    }
    std::unique_ptr<chart::Chart> chart;
    try {
        chart = c.build();
    } catch (...) {
        return stats;  // 建图炸了 = 该候选出局（复核 0 分）
    }
    if (!chart) {
        return stats;
    }
    try {
        canvas::Canvas canvas(width, height);
        chart::DrawContext ctx{canvas};
        chart->draw(ctx);
        const CellBuffer buffer = canvas.to_buffer();
        std::size_t ink = 0;
        std::size_t total = 0;
        std::size_t used_rows = 0;
        for (std::uint32_t y = 0; y < buffer.height(); ++y) {
            bool row_has_ink = false;
            for (const Cell& cell : buffer.row(y)) {
                ++total;
                // 空格与宽字符续接格（U+0000）都不算墨迹。
                if (cell.cp != U' ' && cell.cp != U'\0') {
                    ++ink;
                    row_has_ink = true;
                }
            }
            if (row_has_ink) {
                ++used_rows;
            }
        }
        stats.ink_cells = ink;
        stats.total_cells = total;
        stats.coverage = total == 0 ? 0.0 : static_cast<double>(ink) / static_cast<double>(total);
        stats.row_usage = static_cast<double>(used_rows) / static_cast<double>(height);
    } catch (...) {
        return RasterStats{};  // draw 炸了同上
    }
    return stats;
}

double raster_confirm(const ChartCandidate& c, std::uint32_t width, std::uint32_t height) noexcept {
    const RasterStats stats = raster_stats(c, width, height);
    if (stats.total_cells == 0) {
        return 0.0;
    }
    const double coverage_score = ramp(stats.coverage, kCoverageLo, kCoverageHi);
    const double row_score = ramp(stats.row_usage, kRowUsageLo, kRowUsageHi);
    return clamp01(0.5 * coverage_score + 0.5 * row_score);
}

}  // namespace termchart::autoviz
