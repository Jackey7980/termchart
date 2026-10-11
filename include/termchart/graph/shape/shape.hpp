// shape.hpp —— 节点形状（字符单元格粒度的轮廓）
//
// 形状只描述"哪些格构成边框、用什么字符"，不含填充与样式决策；绘制由 draw_shape
// 完成。轮廓格同时服务于两件事：绘制，以及占用/命中判定。
//
// 精度说明：这里全部是**字符格粒度**的解析轮廓（逐行算左右边界），不是子像素
// 光栅化。曲线（椭圆/胶囊端）在格粒度下必然有台阶，这是终端字符网格的固有限制，
// 不做假精度；需要更平滑的曲线请走 raster/（盲文/象限）那条路径。
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
    Box = 0,    // 直角矩形
    Rounded,    // 圆角矩形
    Diamond,    // 菱形（用于判定节点）
    Ellipse,    // 椭圆（起止节点）
    Stadium,    // 胶囊形（两端半圆，流程图里的"终止"）
    Hexagon,    // 六边形（准备/循环）
    Cylinder,   // 圆柱（数据存储）
    Icon,       // 不画边框：内容由调用方提供（图标/图片节点）
};

// 形状边框占据的格（不含内部；宽高不足 2 时退化为单格或空）。
// Icon 返回空（它没有边框）。
[[nodiscard]] TERMCHART_API std::vector<Point> shape_outline(ShapeKind kind, Rect box);

// 把形状边框画到画布上。越界部分由 canvas 自行裁剪。Icon 不写入任何格。
TERMCHART_API void draw_shape(canvas::Canvas& canvas, ShapeKind kind, Rect box,
                              const Style& style);

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_SHAPE_SHAPE_HPP
