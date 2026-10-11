// hit_region.cpp —— HitRegion 几何判定
#include "termchart/chart/hit_region.hpp"

#include <cmath>
#include <cstddef>
#include <utility>

namespace termchart::chart {

namespace {

// 用点集算外接矩形（Circle/Rect 也统一用它填 bounds_）。
Rect bounds_of(const std::vector<Point>& points) noexcept {
    if (points.empty()) {
        return Rect{};
    }
    double min_x = points[0].x;
    double max_x = points[0].x;
    double min_y = points[0].y;
    double max_y = points[0].y;
    for (const Point& p : points) {
        min_x = p.x < min_x ? p.x : min_x;
        max_x = p.x > max_x ? p.x : max_x;
        min_y = p.y < min_y ? p.y : min_y;
        max_y = p.y > max_y ? p.y : max_y;
    }
    return Rect{min_x, min_y, max_x - min_x, max_y - min_y};
}

// 射线法（奇偶规则）。
bool polygon_contains(const std::vector<Point>& pts, double x, double y) noexcept {
    bool inside = false;
    const std::size_t n = pts.size();
    if (n < 3) {
        return false;
    }
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const double yi = pts[i].y;
        const double yj = pts[j].y;
        const bool straddles = (yi > y) != (yj > y);
        if (!straddles) {
            continue;
        }
        const double xi = pts[i].x;
        const double xj = pts[j].x;
        const double x_cross = xi + (y - yi) / (yj - yi) * (xj - xi);
        if (x < x_cross) {
            inside = !inside;
        }
    }
    return inside;
}

}  // namespace

HitRegion HitRegion::rect(Rect bounds, std::uint32_t payload) {
    HitRegion r;
    r.shape_ = HitShape::Rect;
    r.bounds_ = bounds;
    r.payload_ = payload;
    return r;
}

HitRegion HitRegion::circle(Point center, double radius, std::uint32_t payload) {
    HitRegion r;
    r.shape_ = HitShape::Circle;
    r.radius_ = radius;
    r.bounds_ = Rect{center.x - radius, center.y - radius, radius * 2.0, radius * 2.0};
    r.points_.push_back(center);
    r.payload_ = payload;
    return r;
}

HitRegion HitRegion::polygon(std::vector<Point> points, std::uint32_t payload) {
    HitRegion r;
    r.shape_ = HitShape::Polygon;
    r.bounds_ = bounds_of(points);
    r.points_ = std::move(points);
    r.payload_ = payload;
    return r;
}

bool HitRegion::contains(double x, double y) const noexcept {
    // 先做外接矩形快速否定（所有形状都适用）。
    if (x < bounds_.x || x > bounds_.right() || y < bounds_.y || y > bounds_.bottom()) {
        return false;
    }
    switch (shape_) {
        case HitShape::Rect:
            return true;  // 已过外接矩形
        case HitShape::Circle: {
            const double dx = x - points_[0].x;
            const double dy = y - points_[0].y;
            return dx * dx + dy * dy <= radius_ * radius_;
        }
        case HitShape::Polygon:
            return polygon_contains(points_, x, y);
    }
    return false;
}

}  // namespace termchart::chart
