// grid.cpp —— 网格线实现
#include "termchart/layout/grid.hpp"

#include "termchart/canvas/canvas.hpp"
#include "internal.hpp"

namespace termchart::layout {

void draw_grid(canvas::Canvas& canvas, const PlotArea& area,
               const std::vector<double>& x_ticks,
               const std::vector<double>& y_ticks, const Style& style) {
    const Rect inner = area.inner_rect();
    for (double t : y_ticks) {
        const double y = area.y_scale().forward(t);
        if (y <= inner.y + 0.5 || y >= inner.bottom() - 0.5) {
            continue;  // 与坐标轴重合的线不画，避免压住轴
        }
        canvas.draw_line(Point{inner.x, y}, Point{inner.right(), y}, kGridH, style);
    }
    for (double t : x_ticks) {
        const double x = area.x_scale().forward(t);
        if (x <= inner.x + 0.5 || x >= inner.right() - 0.5) {
            continue;
        }
        canvas.draw_line(Point{x, inner.y}, Point{x, inner.bottom()}, kGridV, style);
    }
}

}  // namespace termchart::layout
