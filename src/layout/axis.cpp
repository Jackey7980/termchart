// axis.cpp —— 坐标轴绘制实现
#include "termchart/layout/axis.hpp"

#include <cmath>

#include "termchart/canvas/canvas.hpp"
#include "internal.hpp"
#include "termchart/layout/label.hpp"

namespace termchart::layout {
namespace {

// 两版 draw_axis_x 的公共体：labels 为空指针 = 走 format_tick。
void draw_x_impl(canvas::Canvas& canvas, const PlotArea& area,
                 const std::vector<double>& ticks, const std::vector<std::string>* labels,
                 std::string_view label, const Style& style) {
    const Rect inner = area.inner_rect();
    const double y = inner.bottom();
    canvas.draw_line(Point{inner.x, y}, Point{inner.right(), y}, kHLine, style);
    canvas.put(Point{inner.x, y}, kCorner, style);

    for (std::size_t i = 0; i < ticks.size(); ++i) {
        const double t = ticks[i];
        const double x = area.x_scale().forward(t);
        if (x < inner.x - 0.5 || x > inner.right() + 0.5) {
            continue;
        }
        canvas.put(Point{x, y}, kTickDown, style);
        const bool has_label = labels != nullptr && i < labels->size() && !(*labels)[i].empty();
        const std::string txt = has_label ? (*labels)[i] : format_tick(t);
        double start = align_start(x - static_cast<double>(txt.size()) / 2.0,
                                   x + static_cast<double>(txt.size()) / 2.0,
                                   txt.size(), HAlign::Center);
        // 末刻度居中会越过右边界，Canvas 按裁剪切掉尾部字符（"03-02 08:00" → "03-"）。
        // 越界时贴右边界右对齐——完整可读优先于严格居中。
        const double max_start = inner.right() + 1.0 - static_cast<double>(txt.size());
        if (start > max_start) {
            start = max_start;
        }
        if (start < inner.x) {
            start = inner.x;
        }
        canvas.draw_text(Point{start, y + 1.0}, txt, style);
    }
    if (!label.empty()) {
        draw_axis_label(canvas, label, inner, Side::Bottom, HAlign::Center, style);
    }
}

}  // namespace

void draw_axis_x(canvas::Canvas& canvas, const PlotArea& area,
                 const std::vector<double>& ticks, std::string_view label,
                 const Style& style) {
    draw_x_impl(canvas, area, ticks, nullptr, label, style);
}

void draw_axis_x(canvas::Canvas& canvas, const PlotArea& area,
                 const std::vector<double>& ticks, const std::vector<std::string>& labels,
                 std::string_view label, const Style& style) {
    draw_x_impl(canvas, area, ticks, &labels, label, style);
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
        // 落在 x 轴那一行的 y 刻度不画刻度钩：它会把左下角的 └ 盖成 ┤。
        // 标签照常画（贴在轴线左侧，不与 x 轴重叠）。
        if (std::fabs(y - inner.bottom()) >= 0.5) {
            canvas.put(Point{x, y}, kTickRight, style);
        }
        const std::string txt = format_tick(t);
        const double start = x - 1.0 - static_cast<double>(txt.size());
        canvas.draw_text(Point{start, y}, txt, style);
    }
    if (!label.empty()) {
        draw_axis_label(canvas, label, inner, Side::Left, HAlign::Center, style);
    }
}

}  // namespace termchart::layout
