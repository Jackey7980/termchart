// path.cpp —— 路径构建与展平实现
#include "termchart/canvas/path.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/canvas/primitives.hpp"
#include "termchart/util/geom.hpp"

namespace termchart::canvas {
namespace {

// 自适应细分深度上限：容差给得极小时防止递归爆炸（2^16 段已远超任何终端分辨率）。
constexpr int kMaxDepth = 16;

// 圆用四段三次贝塞尔逼近时的控制点比例（经典 kappa）。
constexpr double kKappa = 0.5522847498307936;

Point midpoint(Point a, Point b) noexcept {
    return Point{(a.x + b.x) / 2.0, (a.y + b.y) / 2.0};
}

void flatten_quad(Point p0, Point control, Point p1, double tolerance, int depth,
                  std::vector<Point>& out) {
    if (depth <= 0 || util::distance_to_segment(control, p0, p1) <= tolerance) {
        out.push_back(p1);
        return;
    }
    const Point a = midpoint(p0, control);
    const Point b = midpoint(control, p1);
    const Point mid = midpoint(a, b);
    flatten_quad(p0, a, mid, tolerance, depth - 1, out);
    flatten_quad(mid, b, p1, tolerance, depth - 1, out);
}

void flatten_cubic(Point p0, Point c1, Point c2, Point p1, double tolerance, int depth,
                   std::vector<Point>& out) {
    // 平坦度：两个控制点到弦的距离都不超过容差，就认为这段足够直。
    const bool flat = util::distance_to_segment(c1, p0, p1) <= tolerance &&
                      util::distance_to_segment(c2, p0, p1) <= tolerance;
    if (depth <= 0 || flat) {
        out.push_back(p1);
        return;
    }
    const Point a = midpoint(p0, c1);
    const Point b = midpoint(c1, c2);
    const Point c = midpoint(c2, p1);
    const Point d = midpoint(a, b);
    const Point e = midpoint(b, c);
    const Point mid = midpoint(d, e);
    flatten_cubic(p0, a, d, mid, tolerance, depth - 1, out);
    flatten_cubic(mid, e, c, p1, tolerance, depth - 1, out);
}

}  // namespace

void Path::move_to(Point p) {
    PathCommand command;
    command.verb = PathVerb::Move;
    command.a = p;
    commands_.push_back(command);
}

void Path::line_to(Point p) {
    PathCommand command;
    command.verb = PathVerb::Line;
    command.a = p;
    commands_.push_back(command);
}

void Path::quadratic_to(Point control, Point end) {
    PathCommand command;
    command.verb = PathVerb::Quad;
    command.a = control;
    command.b = end;
    commands_.push_back(command);
}

void Path::cubic_to(Point c1, Point c2, Point end) {
    PathCommand command;
    command.verb = PathVerb::Cubic;
    command.a = c1;
    command.b = c2;
    command.c = end;
    commands_.push_back(command);
}

void Path::close() {
    PathCommand command;
    command.verb = PathVerb::Close;
    commands_.push_back(command);
}

void Path::add_rect(const Rect& rect) {
    move_to(Point{rect.x, rect.y});
    line_to(Point{rect.right(), rect.y});
    line_to(Point{rect.right(), rect.bottom()});
    line_to(Point{rect.x, rect.bottom()});
    close();
}

void Path::add_circle(Point center, double radius) {
    if (!(radius > 0.0)) {
        return;
    }
    const double r = radius;
    const double k = radius * kKappa;
    move_to(Point{center.x + r, center.y});
    cubic_to(Point{center.x + r, center.y + k}, Point{center.x + k, center.y + r},
             Point{center.x, center.y + r});
    cubic_to(Point{center.x - k, center.y + r}, Point{center.x - r, center.y + k},
             Point{center.x - r, center.y});
    cubic_to(Point{center.x - r, center.y - k}, Point{center.x - k, center.y - r},
             Point{center.x, center.y - r});
    cubic_to(Point{center.x + k, center.y - r}, Point{center.x + r, center.y - k},
             Point{center.x + r, center.y});
    close();
}

void Path::add_polyline(const std::vector<Point>& points, bool closed) {
    if (points.size() < 2) {
        return;
    }
    move_to(points.front());
    for (std::size_t i = 1; i < points.size(); ++i) {
        line_to(points[i]);
    }
    if (closed) {
        close();
    }
}

std::vector<std::vector<Point>> Path::flatten(double tolerance) const {
    const double tol = tolerance > 0.0 ? tolerance : 0.25;
    std::vector<std::vector<Point>> polylines;
    std::vector<Point> current;
    Point cursor{};
    Point sub_start{};

    // Close 之后再出现 Line/曲线时（没有新的 move_to），新的子路径从当前点起。
    const auto begin_if_needed = [&] {
        if (current.empty()) {
            current.push_back(cursor);
        }
    };
    const auto finish = [&] {
        if (current.size() >= 2) {
            polylines.push_back(current);
        }
        current.clear();
    };

    for (const PathCommand& command : commands_) {
        switch (command.verb) {
            case PathVerb::Move:
                finish();
                cursor = command.a;
                sub_start = command.a;
                current.push_back(cursor);
                break;
            case PathVerb::Line:
                begin_if_needed();
                cursor = command.a;
                current.push_back(cursor);
                break;
            case PathVerb::Quad:
                begin_if_needed();
                flatten_quad(cursor, command.a, command.b, tol, kMaxDepth, current);
                cursor = command.b;
                break;
            case PathVerb::Cubic:
                begin_if_needed();
                flatten_cubic(cursor, command.a, command.b, command.c, tol, kMaxDepth,
                              current);
                cursor = command.c;
                break;
            case PathVerb::Close:
                begin_if_needed();
                cursor = sub_start;
                current.push_back(cursor);
                finish();
                break;
        }
    }
    finish();
    return polylines;
}

Rect Path::bounds() const {
    if (commands_.empty()) {
        return Rect{};
    }
    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    bool first = true;
    const auto extend = [&](Point p) {
        if (first) {
            min_x = max_x = p.x;
            min_y = max_y = p.y;
            first = false;
            return;
        }
        min_x = std::min(min_x, p.x);
        max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y);
        max_y = std::max(max_y, p.y);
    };
    for (const PathCommand& command : commands_) {
        switch (command.verb) {
            case PathVerb::Move:
            case PathVerb::Line:
                extend(command.a);
                break;
            case PathVerb::Quad:
                extend(command.a);
                extend(command.b);
                break;
            case PathVerb::Cubic:
                extend(command.a);
                extend(command.b);
                extend(command.c);
                break;
            case PathVerb::Close:
                break;
        }
    }
    return Rect{min_x, min_y, max_x - min_x, max_y - min_y};
}

std::size_t Path::stroke(Canvas& canvas, char32_t pen, const Style& style,
                         double tolerance) const {
    std::size_t written = 0;
    for (const std::vector<Point>& polyline : flatten(tolerance)) {
        for (std::size_t i = 1; i < polyline.size(); ++i) {
            const std::vector<Point> points = line_points(polyline[i - 1], polyline[i]);
            Point previous{-1.0, -1.0};
            for (const Point& p : points) {
                if (p.x == previous.x && p.y == previous.y) {
                    continue;  // 展平/取整会产出重复点，别把它算两次
                }
                previous = p;
                // put 返回"是否真的落格"：越界由 Canvas 裁剪，这里顺势记账，
                // 不必再手写一遍边界判断（两边判据不一致才是真正的坑）。
                if (canvas.put(p, pen, style)) {
                    ++written;
                }
            }
        }
    }
    return written;
}

std::size_t Path::fill(Canvas& canvas, const FillOptions& opts, double tolerance) const {
    std::size_t written = 0;
    for (const std::vector<Point>& polyline : flatten(tolerance)) {
        if (polyline.size() < 3) {
            continue;
        }
        written += fill_polygon(canvas, polyline, opts);
    }
    return written;
}

}  // namespace termchart::canvas
