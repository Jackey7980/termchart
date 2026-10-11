// fill.cpp —— 扫描线填充实现
#include "termchart/canvas/fill.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace termchart::canvas {
namespace {

constexpr double kSpanEps = 1e-9;

struct Interval {
    double x0 = 0.0;
    double x1 = 0.0;
};

// 对给定采样线 y 求所有 [x0, x1) 区间（奇偶规则）。
// 用半开规则判定"边是否跨过 y"：避免采样线正好穿过顶点时把交点算两次。
std::vector<Interval> intervals_at(const std::vector<Point>& polygon, double y) {
    std::vector<double> xs;
    const std::size_t count = polygon.size();
    for (std::size_t i = 0; i < count; ++i) {
        const Point& a = polygon[i];
        const Point& b = polygon[(i + 1) % count];
        const bool crosses = (a.y > y) != (b.y > y);
        if (!crosses) {
            continue;
        }
        const double t = (y - a.y) / (b.y - a.y);
        xs.push_back(a.x + (b.x - a.x) * t);
    }
    std::sort(xs.begin(), xs.end());

    std::vector<Interval> out;
    out.reserve(xs.size() / 2 + 1);
    for (std::size_t i = 0; i + 1 < xs.size(); i += 2) {
        if (xs[i + 1] - xs[i] > kSpanEps) {
            out.push_back(Interval{xs[i], xs[i + 1]});
        }
    }
    return out;
}

}  // namespace

std::vector<Span> scanline_spans(const std::vector<Point>& polygon) {
    std::vector<Span> spans;
    if (polygon.size() < 3) {
        return spans;
    }
    double min_y = polygon.front().y;
    double max_y = polygon.front().y;
    for (const Point& p : polygon) {
        min_y = std::min(min_y, p.y);
        max_y = std::max(max_y, p.y);
    }
    const auto first_row = static_cast<std::int64_t>(std::floor(min_y));
    const auto last_row = static_cast<std::int64_t>(std::ceil(max_y));
    for (std::int64_t row = first_row; row < last_row; ++row) {
        if (row < 0) {
            continue;  // 画布上没有负行，直接跳过（也不折回第 0 行）
        }
        const double sample_y = static_cast<double>(row) + 0.5;
        for (const Interval& interval : intervals_at(polygon, sample_y)) {
            Span span;
            span.row = static_cast<std::uint32_t>(row);
            span.x0 = interval.x0;
            span.x1 = interval.x1;
            spans.push_back(span);
        }
    }
    return spans;
}

std::size_t fill_polygon(Canvas& canvas, const std::vector<Point>& polygon,
                         const FillOptions& opts) {
    if (polygon.size() < 3) {
        return 0;
    }
    const std::uint32_t sub_rows = std::max<std::uint32_t>(1, opts.supersample);

    double min_y = polygon.front().y;
    double max_y = polygon.front().y;
    for (const Point& p : polygon) {
        min_y = std::min(min_y, p.y);
        max_y = std::max(max_y, p.y);
    }
    const auto first_row = static_cast<std::int64_t>(std::floor(min_y));
    const auto last_row = static_cast<std::int64_t>(std::ceil(max_y));

    std::size_t written = 0;
    for (std::int64_t row = std::max<std::int64_t>(0, first_row); row < last_row; ++row) {
        if (row >= canvas.height()) {
            break;
        }
        // 覆盖度累加器：本行每个候选格累计"被覆盖的纵向子行比例 × 横向重叠"。
        // 用 map 式的稀疏数组会引入哈希开销，这里按行开一个定长数组即可。
        const auto width = canvas.width();
        std::vector<double> coverage(static_cast<std::size_t>(width) + 1, 0.0);

        for (std::uint32_t sub = 0; sub < sub_rows; ++sub) {
            const double sample_y =
                static_cast<double>(row) + (static_cast<double>(sub) + 0.5) /
                                               static_cast<double>(sub_rows);
            for (const Interval& interval : intervals_at(polygon, sample_y)) {
                const double x0 = std::max(0.0, interval.x0);
                const double x1 = std::min(static_cast<double>(width), interval.x1);
                if (x1 - x0 <= kSpanEps) {
                    continue;
                }
                const auto first_col = static_cast<std::size_t>(std::floor(x0));
                const auto last_col = static_cast<std::size_t>(std::ceil(x1));
                for (std::size_t col = first_col; col < last_col && col < width; ++col) {
                    const double left = std::max(x0, static_cast<double>(col));
                    const double right =
                        std::min(x1, static_cast<double>(col) + 1.0);
                    if (right > left) {
                        coverage[col] += (right - left) / static_cast<double>(sub_rows);
                    }
                }
            }
        }

        const auto row_index = static_cast<std::uint32_t>(row);
        for (std::uint32_t col = 0; col < width; ++col) {
            if (coverage[col] >= opts.coverage) {
                canvas.put(col, row_index, opts.on, opts.style);
                ++written;
            }
        }
    }
    return written;
}

}  // namespace termchart::canvas
