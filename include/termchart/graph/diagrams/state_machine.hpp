// state_machine.hpp —— 状态机图（分层布局 + 转移标签）
//
// 与流程图的差别：状态画圆角框、**转移事件名画在边上**（复用 label_place 避让），
// 并按节点属性标注起止状态：
//   attributes["initial"] == "1" → 左侧画 ● 与引入短线
//   attributes["final"]   == "1" → 右侧画 ◎
#ifndef TERMCHART_GRAPH_DIAGRAMS_STATE_MACHINE_HPP
#define TERMCHART_GRAPH_DIAGRAMS_STATE_MACHINE_HPP

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

struct StateMachineOptions {
    LayoutOptions layout{};
    RouteOptions route{};
    RouteStyle route_style = RouteStyle::Orthogonal;
    ShapeKind shape = ShapeKind::Rounded;  // 状态用圆角框
    Style node_style{};
    Style edge_style{};
    bool mark_initial_final = true;
};

// 就地布局并绘制状态机图，返回整图包围盒。
TERMCHART_API Rect render_state_machine(Graph& graph, canvas::Canvas& canvas,
                                        const StateMachineOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_STATE_MACHINE_HPP
