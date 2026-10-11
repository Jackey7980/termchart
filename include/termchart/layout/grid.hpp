// grid.hpp —— 网格线（虚线），在刻度位置铺背景参照
#ifndef TERMCHART_LAYOUT_GRID_HPP
#define TERMCHART_LAYOUT_GRID_HPP

#include <vector>

#include "termchart/core/style.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::layout {

TERMCHART_API void draw_grid(canvas::Canvas& canvas, const PlotArea& area,
                             const std::vector<double>& x_ticks,
                             const std::vector<double>& y_ticks,
                             const Style& style = Style{});

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_GRID_HPP
