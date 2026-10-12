// polar_line.cpp —— 极坐标折线实现（角度 × 半径 → 格坐标 + DDA 连线）
#include "termchart/chart/polar/polar_line.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "termchart/canvas/primitives.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/util/math.hpp"

namespace termchart::chart {
namespace {

constexpr double kTwoPi = 2.0 * util::kPi;

struct PolarFrame {
    Point center;
    double rx = 0.0;
    double ry = 0.0;
};

PolarFrame make_frame(const Rect& inner) noexcept {
    return PolarFrame{inner.center(), inner.w / 2.0, inner.h / 2.0};
}

Point polar_cell(const PolarFrame& f, double rnorm, double swept, double start_rad,
                 bool clockwise) noexcept {
    const double ma = clockwise ? (start_rad - swept) : (start_rad + swept);
    return Point{f.center.x + rnorm * f.rx * std::cos(ma),
                 f.center.y - rnorm * f.ry * std::sin(ma)};
}

void draw_segment(canvas::Canvas& canvas, Point from, Point to, char32_t ch,
                  const Style& st) {
    for (const Point& p : canvas::line_points(from, to)) {
        canvas.put(p, ch, st);
    }
}

}  // namespace

void PolarLineChart::draw(DrawContext ctx) {
    // 只保留有限点：一个 NaN 会把线拖到无从离散。
    std::vector<Point> pts;
    pts.reserve(points_.size());
    for (const Point& p : points_) {
        if (std::isfinite(p.x) && std::isfinite(p.y)) {
            pts.push_back(p);
        }
    }
    if (pts.empty()) {
        return;
    }
    double rmax = options_.rmax;
    if (!(rmax > 0.0)) {
        for (const Point& p : pts) {
            rmax = std::max(rmax, p.y);
        }
    }
    if (!(rmax > 0.0)) {
        rmax = 1.0;  // 半径全零：线缩在圆心，也不除零
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect inner{m.left, m.top,
                     static_cast<double>(ctx.canvas.width()) - m.left - m.right,
                     static_cast<double>(ctx.canvas.height()) - m.top - m.bottom};
    if (inner.w < 3.0 || inner.h < 3.0) {
        return;
    }
    const PolarFrame f = make_frame(inner);
    const double start_rad = util::deg_to_rad(options_.start_angle);
    const Style line_style = Style{}.set_fg(color_);

    // 外圈参考环（满量程）：沿角度采样。
    if (ctx.draw_grid) {
        const Style grid_style = Style{}.set_fg(Color::ansi16(8));
        const int steps =
            std::max(12, static_cast<int>(std::lround(4.0 * (f.rx + f.ry))));
        for (int i = 0; i < steps; ++i) {
            const double swept =
                kTwoPi * static_cast<double>(i) / static_cast<double>(steps);
            ctx.canvas.put(polar_cell(f, 1.0, swept, start_rad, options_.clockwise),
                           U'\u00B7', grid_style);
        }
    }

    // 数据点 → 格坐标（角度归一化到 [0,360)，半径钳到满量程）。
    std::vector<Point> cells;
    cells.reserve(pts.size());
    for (const Point& p : pts) {
        double deg = std::fmod(p.x, 360.0);
        if (deg < 0.0) {
            deg += 360.0;
        }
        const double rnorm = std::clamp(p.y / rmax, 0.0, 1.0);
        cells.push_back(polar_cell(f, rnorm, util::deg_to_rad(deg), start_rad,
                                   options_.clockwise));
    }

    const std::size_t last = cells.size() - 1;
    for (std::size_t i = 0; i < last; ++i) {
        draw_segment(ctx.canvas, cells[i], cells[i + 1], U'\u2022', line_style);
    }
    if (options_.close && cells.size() > 2) {
        draw_segment(ctx.canvas, cells[last], cells[0], U'\u2022', line_style);
    }
    if (options_.show_markers) {
        for (const Point& p : cells) {
            ctx.canvas.put(p, U'\u25CF', line_style);
        }
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

}  // namespace termchart::chart
