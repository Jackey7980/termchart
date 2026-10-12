// gauge.cpp —— 仪表盘实现（角度采样弧 + DDA 指针）
#include "termchart/chart/polar/gauge.hpp"

#include <algorithm>
#include <cmath>

#include "termchart/canvas/primitives.hpp"
#include "termchart/layout/layout.hpp"
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

// 扫角 t ∈ [0,1]（沿绘制方向）→ 格坐标；画布 y 向下为正，故 sin 取负。
Point sweep_cell(const PolarFrame& f, double rnorm, double t, double start_rad,
                 double sweep_rad, bool clockwise) noexcept {
    const double ma = clockwise ? (start_rad - t * sweep_rad)
                                : (start_rad + t * sweep_rad);
    return Point{f.center.x + rnorm * f.rx * std::cos(ma),
                 f.center.y - rnorm * f.ry * std::sin(ma)};
}

}  // namespace

void GaugeChart::draw(DrawContext ctx) {
    // 值域退化（min ≥ max 或非有限）：没有映射可言，不画。
    if (!(options_.value_max > options_.value_min) ||
        !std::isfinite(options_.value_min) || !std::isfinite(options_.value_max)) {
        return;
    }
    double sweep_deg = options_.clockwise ? (options_.start_angle - options_.end_angle)
                                          : (options_.end_angle - options_.start_angle);
    if (!(sweep_deg > 0.0) || sweep_deg > 360.0) {
        sweep_deg = 360.0;  // 端点写反/相等时退化为整圆，而不是消失
    }
    const double sweep_rad = util::deg_to_rad(sweep_deg);
    const double start_rad = util::deg_to_rad(options_.start_angle);

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect inner{m.left, m.top,
                     static_cast<double>(ctx.canvas.width()) - m.left - m.right,
                     static_cast<double>(ctx.canvas.height()) - m.top - m.bottom};
    if (inner.w < 3.0 || inner.h < 3.0) {
        return;
    }
    const PolarFrame f = make_frame(inner);

    const Style gauge_style = Style{}.set_fg(color_);
    const Style tick_style = Style{}.set_fg(Color::ansi16(8));

    // 弧体：按角度采样，步长随周长走，保证相邻采样点格间连续。
    const int steps = std::max(24, static_cast<int>(std::lround(
                                      4.0 * (f.rx + f.ry) * sweep_deg / 360.0)));
    for (int i = 0; i <= steps; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(steps);
        ctx.canvas.put(sweep_cell(f, 1.0, t, start_rad, sweep_rad, options_.clockwise),
                       U'\u25CF', gauge_style);
    }

    // 主刻度：'+' 压在弧体上并向内再点一粒（rnorm 0.88），读出方向感。
    const int ticks = std::clamp(options_.tick_count, 2, 64);
    for (int k = 0; k < ticks; ++k) {
        const double t = static_cast<double>(k) / static_cast<double>(ticks - 1);
        ctx.canvas.put(sweep_cell(f, 1.0, t, start_rad, sweep_rad, options_.clockwise),
                       U'+', tick_style);
        ctx.canvas.put(sweep_cell(f, 0.88, t, start_rad, sweep_rad, options_.clockwise),
                       U'+', tick_style);
    }

    // 指针：圆心 → rnorm 0.72 的 DDA 线段；值钳到值域，非有限值不画指针。
    if (std::isfinite(value_)) {
        const double t = std::clamp((value_ - options_.value_min) /
                                        (options_.value_max - options_.value_min),
                                    0.0, 1.0);
        const Point tip = sweep_cell(f, 0.72, t, start_rad, sweep_rad, options_.clockwise);
        for (const Point& p : canvas::line_points(f.center, tip)) {
            ctx.canvas.put(p, U'*', gauge_style);
        }
    }
    ctx.canvas.put(f.center, U'\u25CF', gauge_style);  // 轴帽

    if (options_.show_value) {
        const std::string text = layout::format_tick(value_);
        ctx.canvas.draw_text(Point{f.center.x - 2.0, f.center.y + 2.0}, text,
                             Style{});
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

}  // namespace termchart::chart
