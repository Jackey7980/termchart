// axis.cpp —— 坐标轴绘制实现
#include "termchart/layout/axis.hpp"

#include "termchart/canvas/canvas.hpp"
#include "internal.hpp"
#include "termchart/layout/label.hpp"

namespace termchart::layout {

void draw_axis_x(canvas::Canvas& canvas, const PlotArea& area,
                 const std::vector<double>& ticks, std::string_view label,
                 const Style& style) {
    const Rect inner = area.inner_rect();
    const double y = inner.bottom();
    canvas.draw_line(Point{inner.x, y}, Point{inner.right(), y}, kHLine, style);
    canvas.put(Point{inner.x, y}, kCorner, style);

    for (double t : ticks) {
        const double x = area.x_scale().forward(t);
        if (x < inner.x - 0.5 || x > inner.right() + 0.5) {
            continue;
        }
        canvas.put(Point{x, y}, kTickDown, style);
        const std::string txt = format_tick(t);
        const double start = align_start(x - static_cast<double>(txt.size()) / 2.0,
                                         x + static_cast<double>(txt.size()) / 2.0,
                                         txt.size(), HAlign::Center);
        canvas.draw_text(Point{start, y + 1.0}, txt, style);
    }
    if (!label.empty()) {
        draw_axis_label(canvas, label, inner, Side::Bottom, HAlign::Center, style);
    }
}

void draw_axis_y(canvas::Canvas& canvas, const PlotArea& area,
                 const std::vector<double>& ticks, std::string_view label,
                 const Style& style) {
    const Rect inner = area.inner_rect();
    const double x = inner.x;
    canvas.draw_line(Point{x, inner.y}, Point{x, inner.bottom()}, kVLine, style);
    canvas.put(Point{x, inner.bottom()}, kCorner, style);

    for (double t : ticks) {
        const double y = area.y_scale().forward(t);
        if (y < inner.y - 0.5 || y > inner.bottom() + 0.5) {
            continue;
        }
        canvas.put(Point{x, y}, kTickRight, style);
        const std::string txt = format_tick(t);
        const double start = x - 1.0 - static_cast<double>(txt.size());
        canvas.draw_text(Point{start, y}, txt, style);
    }
    if (!label.empty()) {
        draw_axis_label(canvas, label, inner, Side::Left, HAlign::Center, style);
    }
}

}  // namespace termchart::layout
