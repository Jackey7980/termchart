// geom.cpp —— 平面几何实现
#include "termchart/util/geom.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace termchart::util {
namespace {

// 判定"点落在线段上"的容差。几何量都是格坐标（整数级），1e-9 足够区分
// "真的在线上"与"差一点点"，又不会被浮点噪声误判。
constexpr double kOnEdgeEps = 1e-9;

bool point_on_segment(Point p, Point a, Point b) noexcept {
    return distance_to_segment(p, a, b) <= kOnEdgeEps;
}

}  // namespace

double cross(Point a, Point b, Point c) noexcept {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

double signed_distance_to_line(Point p, Point a, Point b) noexcept {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.0) {
        return std::hypot(p.x - a.x, p.y - a.y);
    }
    return cross(a, b, p) / len;
}

double distance_to_segment(Point p, Point a, Point b) noexcept {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double length_sq = dx * dx + dy * dy;
    if (length_sq <= 0.0) {
        return std::hypot(p.x - a.x, p.y - a.y);  // 退化成点
    }
    double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / length_sq;
    t = std::clamp(t, 0.0, 1.0);
    const Point closest{a.x + t * dx, a.y + t * dy};
    return std::hypot(p.x - closest.x, p.y - closest.y);
}

bool segments_cross(Point a, Point b, Point c, Point d) noexcept {
    const double d1 = cross(c, d, a);
    const double d2 = cross(c, d, b);
    const double d3 = cross(a, b, c);
    const double d4 = cross(a, b, d);
    // 必须**两侧都严格反向**才算真交叉。用"符号异或"会在 d==0（端点贴线、
    // 共线）时误判成相交，所以这里显式要求一正一负。
    const bool ab_opposite = (d1 > 0.0 && d2 < 0.0) || (d1 < 0.0 && d2 > 0.0);
    const bool cd_opposite = (d3 > 0.0 && d4 < 0.0) || (d3 < 0.0 && d4 > 0.0);
    return ab_opposite && cd_opposite;
}

bool segment_intersection(Point a, Point b, Point c, Point d, Point& out) noexcept {
    const double rx = b.x - a.x;
    const double ry = b.y - a.y;
    const double sx = d.x - c.x;
    const double sy = d.y - c.y;
    const double denom = rx * sy - ry * sx;
    if (std::fabs(denom) <= 0.0) {
        return false;  // 平行、共线，或其中一条退化成点
    }
    const double qpx = c.x - a.x;
    const double qpy = c.y - a.y;
    const double t = (qpx * sy - qpy * sx) / denom;
    const double u = (qpx * ry - qpy * rx) / denom;
    if (t < 0.0 || t > 1.0 || u < 0.0 || u > 1.0) {
        return false;
    }
    out = Point{a.x + t * rx, a.y + t * ry};
    return true;
}

double polygon_area(const Point* points, std::size_t count) noexcept {
    if (points == nullptr || count < 3) {
        return 0.0;
    }
    double twice = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const Point& p = points[i];
        const Point& q = points[(i + 1) % count];
        twice += p.x * q.y - q.x * p.y;
    }
    return twice / 2.0;
}

Point polygon_centroid(const Point* points, std::size_t count) noexcept {
    if (points == nullptr || count == 0) {
        return Point{0.0, 0.0};
    }
    double twice = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const Point& p = points[i];
        const Point& q = points[(i + 1) % count];
        const double term = p.x * q.y - q.x * p.y;
        twice += term;
        cx += (p.x + q.x) * term;
        cy += (p.y + q.y) * term;
    }
    if (std::fabs(twice) <= kOnEdgeEps) {
        // 面积退化（共线/重复点）：退回到顶点算术平均，至少给个稳定结果。
        double sx = 0.0;
        double sy = 0.0;
        for (std::size_t i = 0; i < count; ++i) {
            sx += points[i].x;
            sy += points[i].y;
        }
        return Point{sx / static_cast<double>(count), sy / static_cast<double>(count)};
    }
    return Point{cx / (3.0 * twice), cy / (3.0 * twice)};
}

bool point_in_polygon(Point p, const Point* points, std::size_t count) noexcept {
    if (points == nullptr || count < 3) {
        return false;
    }
    bool inside = false;
    for (std::size_t i = 0, j = count - 1; i < count; j = i++) {
        const Point& a = points[i];
        const Point& b = points[j];
        if (point_on_segment(p, a, b)) {
            return true;  // 边界算内部
        }
        // 射线法：只统计跨越 p.y 的边，且用半开区间避免顶点被数两次。
        if ((a.y > p.y) != (b.y > p.y)) {
            const double x_at = (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x;
            if (p.x < x_at) {
                inside = !inside;
            }
        }
    }
    return inside;
}

std::vector<Point> convex_hull(std::vector<Point> points) {
    std::sort(points.begin(), points.end(), [](const Point& lhs, const Point& rhs) {
        return lhs.x < rhs.x || (lhs.x == rhs.x && lhs.y < rhs.y);
    });
    points.erase(std::unique(points.begin(), points.end(),
                             [](const Point& lhs, const Point& rhs) {
                                 return lhs.x == rhs.x && lhs.y == rhs.y;
                             }),
                 points.end());
    if (points.size() < 3) {
        return points;
    }

    // 单调链：先右链（保留下凸），再左链（保留下凹），共线点用 <= 0 剔除。
    std::vector<Point> hull(points.size() * 2);
    std::size_t size = 0;
    for (const Point& p : points) {
        while (size >= 2 && cross(hull[size - 2], hull[size - 1], p) <= 0.0) {
            --size;
        }
        hull[size++] = p;
    }
    const std::size_t lower = size + 1;
    for (std::size_t i = points.size() - 1; i-- > 0;) {
        const Point& p = points[i];
        while (size >= lower && cross(hull[size - 2], hull[size - 1], p) <= 0.0) {
            --size;
        }
        hull[size++] = p;
    }
    hull.resize(size > 0 ? size - 1 : 0);  // 末点与起点重复
    return hull;
}

}  // namespace termchart::util
