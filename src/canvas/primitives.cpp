#include "termchart/canvas/primitives.hpp"

#include <algorithm>
#include <cmath>

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

void append_unique(std::vector<Point>& out, Point p) noexcept {
    const auto exists = std::any_of(out.begin(), out.end(), [&p](const Point& q) {
        return std::floor(q.x) == std::floor(p.x) && std::floor(q.y) == std::floor(p.y);
    });
    if (!exists) {
        out.push_back(Point{std::floor(p.x), std::floor(p.y)});
    }
}

}  // namespace

std::vector<Point> line_points(Point from, Point to) noexcept {
    std::vector<Point> out;
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
        append_unique(out, Point{from.x + dx * t, from.y + dy * t});
    }
    return out;
}

std::vector<Point> rect_outline_points(Rect rect) noexcept {
    std::vector<Point> out;
    if (rect.empty()) {
        return out;
    }
    const double x0 = rect.x;
    const double y0 = rect.y;
    const double x1 = rect.right();
    const double y1 = rect.bottom();
    // 四条边按"角落在前一条边里已写入"的顺序，避免重复写角。
    for (const Point& p : line_points(Point{x0, y0}, Point{x1, y0})) {
        append_unique(out, p);
    }
    for (const Point& p : line_points(Point{x1, y0}, Point{x1, y1})) {
        append_unique(out, p);
    }
    for (const Point& p : line_points(Point{x1, y1}, Point{x0, y1})) {
        append_unique(out, p);
    }
    for (const Point& p : line_points(Point{x0, y1}, Point{x0, y0})) {
        append_unique(out, p);
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
    for (std::size_t i = 0; i < steps; ++i) {
        const double angle = start + span * (static_cast<double>(i) / static_cast<double>(steps));
        append_unique(out, Point{center.x + std::cos(angle) * radius,
                                 center.y + std::sin(angle) * radius});
    }
    return out;
}

}  // namespace termchart::canvas
