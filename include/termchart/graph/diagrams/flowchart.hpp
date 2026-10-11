// flowchart.hpp —— 流程图（图 --> 画布 的一步到位入口）
//
// 组合关系固定为：分层布局 -> 路由（正交/直线）-> 绘制形状、标签、箭头。
// 需要单独控制某一步时，直接调用 graph/layout、graph/route、graph/shape。
#ifndef TERMCHART_GRAPH_DIAGRAMS_FLOWCHART_HPP
#define TERMCHART_GRAPH_DIAGRAMS_FLOWCHART_HPP

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"
#include "termchart/graph/route/route.hpp"
#include "termchart/graph/shape/shape.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct FlowchartOptions {
    LayoutOptions layout{};
    RouteOptions route{};
    RouteStyle route_style = RouteStyle::Orthogonal;
    ShapeKind shape = ShapeKind::Box;
    Style node_style{};
    Style edge_style{};
};

// 就地布局并绘制到画布（超出画布的部分由 canvas 裁剪）。返回整图包围盒。
TERMCHART_API Rect render_flowchart(Graph& graph, canvas::Canvas& canvas,
                                    const FlowchartOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_FLOWCHART_HPP
