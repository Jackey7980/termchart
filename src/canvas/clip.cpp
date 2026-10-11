// clip.cpp —— 裁剪区域实现
#include "termchart/canvas/clip.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "termchart/util/geom.hpp"

namespace termchart::canvas {
namespace {

constexpr double kEps = 1e-9;

// 线段与半平面边界的参数交点：返回 a→b 上使 half_plane(a,b,p) == 0 的 t。
double crossing_t(Point a, Point b, Point edge_a, Point edge_b) noexcept {
    const double da = util::cross(edge_a, edge_b, a);
    const double db = util::cross(edge_a, edge_b, b);
    const double denom = da - db;
    if (std::fabs(denom) <= kEps) {
        return 0.0;  // 平行于边界：取起点（调用方只在明确一内一外时使用）
    }
    return da / denom;
}

}  // namespace

ClipRegion ClipRegion::rect(const Rect& bounds) {
    ClipRegion region;
    region.has_rect_ = true;
    region.rect_ = bounds;
    return region;
}

ClipRegion ClipRegion::polygon(std::vector<Point> points) {
    ClipRegion region;
    if (points.size() < 3) {
        return region;  // 少于 3 点无法构成区域：退化成"无约束"
    }
    // 规范化成逆时针：面积为负说明是顺时针，反转。
    if (util::polygon_area(points.data(), points.size()) < 0.0) {
        std::reverse(points.begin(), points.end());
    }
    region.has_polygon_ = true;
    region.polygon_ = std::move(points);

    // 凸性判定：逆时针多边形每个相邻边的叉积都非负。
    bool convex = true;
    const std::size_t count = region.polygon_.size();
    for (std::size_t i = 0; i < count && convex; ++i) {
        const Point& a = region.polygon_[i];
        const Point& b = region.polygon_[(i + 1) % count];
        const Point& c = region.polygon_[(i + 2) % count];
        if (util::cross(a, b, c) < -kEps) {
            convex = false;
        }
    }
    region.polygon_convex_ = convex;
    return region;
}

bool ClipRegion::contains(Point p) const noexcept {
    if (has_rect_) {
        return p.x >= rect_.x && p.x <= rect_.right() && p.y >= rect_.y &&
               p.y <= rect_.bottom();
    }
    if (has_polygon_) {
        return util::point_in_polygon(p, polygon_.data(), polygon_.size());
    }
    return true;  // 无约束
}

bool ClipRegion::supports_polygon_clip() const noexcept {
    if (has_rect_) {
        return true;
    }
    if (has_polygon_) {
        return polygon_convex_;
    }
    return true;  // 无约束：原样返回即可
}

void ClipRegion::clip_half_plane(std::vector<Point>& poly, Point edge_a, Point edge_b,
                                 bool keep_left) const {
    if (poly.empty()) {
        return;
    }
    std::vector<Point> out;
    out.reserve(poly.size() + 2);
    const auto inside = [&](Point p) {
        const double side = util::cross(edge_a, edge_b, p);
        return keep_left ? side >= -kEps : side <= kEps;
    };
    const std::size_t count = poly.size();
    for (std::size_t i = 0; i < count; ++i) {
        const Point current = poly[i];
        const Point next = poly[(i + 1) % count];
        const bool current_in = inside(current);
        const bool next_in = inside(next);
        if (current_in) {
            out.push_back(current);
        }
        if (current_in != next_in) {
            const double t = crossing_t(current, next, edge_a, edge_b);
            out.push_back(Point{current.x + (next.x - current.x) * t,
                                current.y + (next.y - current.y) * t});
        }
    }
    poly.swap(out);
}

std::vector<Point> ClipRegion::clip_polygon(std::vector<Point> polygon) const {
    if (polygon.size() < 3) {
        return polygon;
    }
    if (has_rect_) {
        // 矩形 = 四个半平面（逆时针绕行）。
        const Point corners[4] = {
            Point{rect_.x, rect_.y},
            Point{rect_.right(), rect_.y},
            Point{rect_.right(), rect_.bottom()},
            Point{rect_.x, rect_.bottom()},
        };
        for (int i = 0; i < 4 && !polygon.empty(); ++i) {
            clip_half_plane(polygon, corners[i], corners[(i + 1) % 4], true);
        }
        return polygon;
    }
    if (has_polygon_ && polygon_convex_) {
        const std::size_t count = polygon_.size();
        for (std::size_t i = 0; i < count && !polygon.empty(); ++i) {
            // 逆时针多边形：边 a→b 的左侧即内部。
            clip_half_plane(polygon, polygon_[i], polygon_[(i + 1) % count], true);
        }
        return polygon;
    }
    return polygon;  // 非凸区域不支持多边形裁剪：原样返回
}

bool ClipRegion::clip_segment(Point a, Point b, Point& out_a, Point& out_b) const noexcept {
    out_a = a;
    out_b = b;
    if (unconstrained()) {
        return true;
    }

    if (has_rect_) {
        // Liang–Barsky：按参数区间逐步收紧。
        double t0 = 0.0;
        double t1 = 1.0;
        const double dx = b.x - a.x;
        const double dy = b.y - a.y;
        const double p[4] = {-dx, dx, -dy, dy};
        const double q[4] = {a.x - rect_.x, rect_.right() - a.x, a.y - rect_.y,
                             rect_.bottom() - a.y};
        for (int i = 0; i < 4; ++i) {
            if (std::fabs(p[i]) <= kEps) {
                if (q[i] < 0.0) {
                    return false;  // 平行于该边界且在外侧
                }
                continue;
            }
            const double r = q[i] / p[i];
            if (p[i] < 0.0) {
                t0 = std::max(t0, r);
            } else {
                t1 = std::min(t1, r);
            }
            if (t0 > t1) {
                return false;
            }
        }
        out_a = Point{a.x + dx * t0, a.y + dy * t0};
        out_b = Point{a.x + dx * t1, a.y + dy * t1};
        return true;
    }

    // 凸多边形：取参数候选点（端点 + 与各边的交点），在每段中点判定内外，
    // 取第一段到最后一"在内"段的并集（凸区域内部必是连续区间）。
    if (!polygon_convex_) {
        // 非凸区域没有精确解，退回"两端都在内才保留"的保守判定。
        return contains(a) && contains(b);
    }
    const std::size_t count = polygon_.size();
    std::vector<double> ts;
    ts.reserve(count + 2);
    ts.push_back(0.0);
    ts.push_back(1.0);
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    for (std::size_t i = 0; i < count; ++i) {
        const Point& ea = polygon_[i];
        const Point& eb = polygon_[(i + 1) % count];
        const double da = util::cross(ea, eb, a);
        const double db = util::cross(ea, eb, b);
        if ((da > 0.0) != (db > 0.0) && std::fabs(da - db) > kEps) {
            ts.push_back(da / (da - db));
        }
    }
    std::sort(ts.begin(), ts.end());
    double first_in = -1.0;
    double last_in = -1.0;
    for (std::size_t i = 0; i + 1 < ts.size(); ++i) {
        if (ts[i + 1] - ts[i] <= kEps) {
            continue;
        }
        const double mid = (ts[i] + ts[i + 1]) / 2.0;
        const Point probe{a.x + dx * mid, a.y + dy * mid};
        if (contains(probe)) {
            if (first_in < 0.0) {
                first_in = ts[i];
            }
            last_in = ts[i + 1];
        }
    }
    if (first_in < 0.0) {
        return false;
    }
    out_a = Point{a.x + dx * first_in, a.y + dy * first_in};
    out_b = Point{a.x + dx * last_in, a.y + dy * last_in};
    return true;
}

}  // namespace termchart::canvas
