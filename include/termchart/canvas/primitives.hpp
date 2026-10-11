// primitives.hpp —— 图元离散化（浮点 → 格坐标序列）
//
// 纯函数层：**只产出"要写哪些格"**，不碰 Canvas，也不决定用什么字符。
// 于是光栅器（raster/）将来可以在同一份离散结果上换字符，而图元算法不变。
#ifndef TERMCHART_CANVAS_PRIMITIVES_HPP
#define TERMCHART_CANVAS_PRIMITIVES_HPP

#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"

namespace termchart::canvas {

// 直线：DDA 采样，步长取一格；返回按行走的格坐标（已向下取整，可能重复）。
TERMCHART_API std::vector<Point> line_points(Point from, Point to) noexcept;

// 矩形轮廓（四条边，角落不重复）。
TERMCHART_API std::vector<Point> rect_outline_points(Rect rect) noexcept;

// 整圆 / 圆弧：按角度采样后去重。起止角为弧度，默认整圆 [0, 2π)。
TERMCHART_API std::vector<Point> circle_points(Point center, double radius) noexcept;
TERMCHART_API std::vector<Point> arc_points(Point center, double radius, double start,
                                            double end) noexcept;

}  // namespace termchart::canvas

#endif  // TERMCHART_CANVAS_PRIMITIVES_HPP
