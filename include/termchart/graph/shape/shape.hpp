// shape.hpp —— 节点形状（字符单元格粒度的轮廓）
//
// 形状只描述"哪些格构成边框、用什么字符"，不含填充与样式决策；绘制由
// draw_shape 完成。轮廓格同时服务于两件事：绘制，以及占用/命中判定。
// 说明：真正的椭圆边框需要子像素光栅化（raster/），字符粒度下不做近似，
// 因此这里只提供 Box / Rounded / Diamond 三种有把握的形状。
#ifndef TERMCHART_GRAPH_SHAPE_SHAPE_HPP
#define TERMCHART_GRAPH_SHAPE_SHAPE_HPP

#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

enum class ShapeKind {
    Box = 0,      // 直角矩形
    Rounded,      // 圆角矩形
    Diamond,      // 菱形（用于判定节点）
};

// 形状边框占据的格（不含内部；宽高不足 2 时退化为单格或空）。
[[nodiscard]] TERMCHART_API std::vector<Point> shape_outline(ShapeKind kind, Rect box);

// 把形状边框画到画布上。越界部分由 canvas 自行裁剪。
TERMCHART_API void draw_shape(canvas::Canvas& canvas, ShapeKind kind, Rect box,
                              const Style& style);

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_SHAPE_SHAPE_HPP
