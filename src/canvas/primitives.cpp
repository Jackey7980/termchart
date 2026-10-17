#include "termchart/canvas/primitives.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace termchart::canvas {
namespace {

constexpr double kTwoPi = 6.28318530717958647692;

// 采样密度：每格至少一个采样点，保证线不出现断口。
std::size_t steps_for(double dx, double dy) noexcept {
    const double span = std::max(std::fabs(dx), std::fabs(dy));
    if (!(span > 0.0)) {
        return 1;
    }
    return static_cast<std::size_t>(std::ceil(span)) + 1;
}

// 去重键：把下取整后的格坐标压成 64 位整数。x/y 各自取 int64 低 32 位
//（含负数，保证与原 any_of 的 floor(x)==floor(y) 语义完全一致），不同格绝不碰撞。
uint64_t cell_key(Point p) noexcept {
    const auto fx = static_cast<std::int64_t>(std::floor(p.x));
    const auto fy = static_cast<std::int64_t>(std::floor(p.y));
    const auto ux = static_cast<std::uint64_t>(fx) & 0xFFFFFFFFull;
    const auto uy = static_cast<std::uint64_t>(fy) & 0xFFFFFFFFull;
    return ux | (uy << 32);
}

// 用哈希集做 O(1) 去重，把整条线的复杂度从 O(N^2) 降到 O(N)。
// 光栅化（Canvas::put）本身幂等、重复相邻点无害；此处去重只为收敛输出规模。
inline void append_unique(std::unordered_set<uint64_t>& seen, std::vector<Point>& out,
                          Point p) noexcept {
    if (seen.insert(cell_key(p)).second) {
        out.push_back(Point{std::floor(p.x), std::floor(p.y)});
    }
}

}  // namespace

std::vector<Point> line_points(Point from, Point to) noexcept {
    std::vector<Point> out;
    std::unordered_set<uint64_t> seen;
    const double dx = to.x - from.x;
    const double dy = to.y - from.y;
    const std::size_t steps = steps_for(dx, dy);
    if (steps <= 1) {
        // 退化成一点：直接落格，避免 0/0 产生 NaN。
        out.push_back(Point{std::floor(from.x), std::floor(from.y)});
        return out;
    }
    out.reserve(steps);
    for (std::size_t i = 0; i < steps; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(steps - 1);
        append_unique(seen, out, Point{from.x + dx * t, from.y + dy * t});
    }
    return out;
}

std::vector<Point> rect_outline_points(Rect rect) noexcept {
    std::vector<Point> out;
    if (rect.empty()) {
        return out;
    }
    std::unordered_set<uint64_t> seen;
    const double x0 = rect.x;
    const double y0 = rect.y;
    const double x1 = rect.right();
    const double y1 = rect.bottom();
    // 四条边用同一把 seen，连共享角也去重（保持原行为）。
    for (const Point& p : line_points(Point{x0, y0}, Point{x1, y0})) {
        append_unique(seen, out, p);
    }
    for (const Point& p : line_points(Point{x1, y0}, Point{x1, y1})) {
        append_unique(seen, out, p);
    }
    for (const Point& p : line_points(Point{x1, y1}, Point{x0, y1})) {
        append_unique(seen, out, p);
    }
    for (const Point& p : line_points(Point{x0, y1}, Point{x0, y0})) {
        append_unique(seen, out, p);
    }
    return out;
}

std::vector<Point> circle_points(Point center, double radius) noexcept {
    return arc_points(center, radius, 0.0, kTwoPi);
}

std::vector<Point> arc_points(Point center, double radius, double start, double end) noexcept {
    std::vector<Point> out;
    if (!(radius > 0.0) || !(end > start)) {
        return out;
    }
    // 采样步长取"每格一个点"：圆周上约 2πr 个格，多一点保证闭合。
    const double span = end - start;
    const std::size_t steps = static_cast<std::size_t>(std::ceil(span * radius)) + 8;
    out.reserve(steps);
    std::unordered_set<uint64_t> seen;
    for (std::size_t i = 0; i < steps; ++i) {
        const double angle = start + span * (static_cast<double>(i) / static_cast<double>(steps));
        append_unique(seen, out, Point{center.x + std::cos(angle) * radius,
                                       center.y + std::sin(angle) * radius});
    }
    return out;
}

}  // namespace termchart::canvas
