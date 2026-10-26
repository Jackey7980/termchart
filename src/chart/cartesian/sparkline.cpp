// sparkline.cpp —— 迷你图实现（单行下八分块，逐列线性重采样）
#include "termchart/chart/cartesian/sparkline.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "termchart/canvas/canvas.hpp"
#include "termchart/detail/block_chars.hpp"
#include "termchart/chart/scene_sink.hpp"

namespace termchart::chart {

void SparklineChart::set_values(std::vector<double> values) { values_ = std::move(values); }

void SparklineChart::draw(DrawContext ctx) {
    std::vector<double> vals;
    vals.reserve(values_.size());
    for (const double v : values_) {
        if (std::isfinite(v)) {
            vals.push_back(v);
        }
    }
    if (vals.empty()) {
        return;
    }

    const auto w = static_cast<int>(ctx.canvas.width());
    if (w < 1) {
        return;
    }

    double vmin = vals.front();
    double vmax = vals.front();
    for (const double v : vals) {
        vmin = std::min(vmin, v);
        vmax = std::max(vmax, v);
    }
    const double span = vmax - vmin;  // 0 表示常值序列：全部画半高档

    const Style st = Style{}.set_fg(options_.color);
    const auto n = static_cast<int>(vals.size());
    for (int col = 0; col < w; ++col) {
        // 列 → 数据索引的线性重采样：单点或单列直接取中位。
        double u = 0.0;
        if (n > 1 && w > 1) {
            u = static_cast<double>(col) * static_cast<double>(n - 1) /
                static_cast<double>(w - 1);
        } else {
            u = static_cast<double>(n - 1) / 2.0;
        }
        const int i0 = std::clamp(static_cast<int>(u), 0, n - 1);
        const int i1 = std::clamp(i0 + (u > static_cast<double>(i0) ? 1 : 0), 0, n - 1);
        const double frac = u - static_cast<double>(i0);
        const double v = vals[static_cast<std::size_t>(i0)] * (1.0 - frac) +
                         vals[static_cast<std::size_t>(i1)] * frac;

        const double t = span > 0.0 ? (v - vmin) / span : 0.5;
        // 至少 1/8 高：有数据的列绝不画成空格，否则"低值"与"缺失"无法区分。
        const int level = std::clamp(static_cast<int>(t * 8.0 + 0.5), 1, 8);
        ctx.canvas.put(Point{static_cast<double>(col), 0.0}, detail::lower_eighth(level),
                       st);
    }

    if (options_.show_last_marker && w > 0) {
        ctx.canvas.put(Point{static_cast<double>(w - 1), 0.0}, kMarkerChar, st);
    }
}

bool SparklineChart::to_scene(scene::Scene& out, std::uint32_t width,
                              std::uint32_t height) const {
    (void)height;  // 迷你图只占单行，几何与高度无关
    // 与 draw() 同源：剔除非有限值，逐列线性重采样为下八分块。
    std::vector<double> vals;
    vals.reserve(values_.size());
    for (const double v : values_) {
        if (std::isfinite(v)) {
            vals.push_back(v);
        }
    }
    if (vals.empty()) {
        return true;
    }

    const auto w = static_cast<int>(width);
    if (w < 1) {
        return true;
    }

    double vmin = vals.front();
    double vmax = vals.front();
    for (const double v : vals) {
        vmin = std::min(vmin, v);
        vmax = std::max(vmax, v);
    }
    const double span = vmax - vmin;  // 0 表示常值序列：全部画半高档

    const Style st = Style{}.set_fg(options_.color);
    const auto n = static_cast<int>(vals.size());
    SceneSink sink(out);
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);
    for (int col = 0; col < w; ++col) {
        // 列 → 数据索引的线性重采样：单点或单列直接取中位（与 draw() 同源）。
        double u = 0.0;
        if (n > 1 && w > 1) {
            u = static_cast<double>(col) * static_cast<double>(n - 1) /
                static_cast<double>(w - 1);
        } else {
            u = static_cast<double>(n - 1) / 2.0;
        }
        const int i0 = std::clamp(static_cast<int>(u), 0, n - 1);
        const int i1 = std::clamp(i0 + (u > static_cast<double>(i0) ? 1 : 0), 0, n - 1);
        const double frac = u - static_cast<double>(i0);
        const double v = vals[static_cast<std::size_t>(i0)] * (1.0 - frac) +
                         vals[static_cast<std::size_t>(i1)] * frac;

        const double t = span > 0.0 ? (v - vmin) / span : 0.5;
        // 至少 1/8 高：有数据的列绝不画成空格，否则"低值"与"缺失"无法区分。
        const int level = std::clamp(static_cast<int>(t * 8.0 + 0.5), 1, 8);
        sink.put(Point{static_cast<double>(col), 0.0}, detail::lower_eighth(level), st);
    }

    if (options_.show_last_marker && w > 0) {
        sink.put(Point{static_cast<double>(w - 1), 0.0}, kMarkerChar, st);
    }
    b.end_group(0);
    return true;
}

}  // namespace termchart::chart
