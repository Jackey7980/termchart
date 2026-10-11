// axis.hpp —— 坐标轴绘制（左 y 轴 / 下 x 轴）：轴体 + 刻度 + 数字标签 + 轴名
#ifndef TERMCHART_LAYOUT_AXIS_HPP
#define TERMCHART_LAYOUT_AXIS_HPP

#include <string_view>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::layout {

// 下 x 轴：在 inner 底边画横线，向上长出刻度并标注数值；label 居中置于轴下方。
TERMCHART_API void draw_axis_x(canvas::Canvas& canvas, const PlotArea& area,
                               const std::vector<double>& ticks,
                               std::string_view label = {},
                               const Style& style = Style{});

// 左 y 轴：在 inner 左边画竖线，向右长出刻度并标注数值；label 置于轴左侧。
TERMCHART_API void draw_axis_y(canvas::Canvas& canvas, const PlotArea& area,
                               const std::vector<double>& ticks,
                               std::string_view label = {},
                               const Style& style = Style{});

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_AXIS_HPP
