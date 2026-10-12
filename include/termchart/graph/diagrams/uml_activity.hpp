// uml_activity.hpp —— UML 活动图（圆角动作框 + 菱形判定 + ●/◎ 起止标记）
//
// 字符化策略：动作 = 圆角框；判定节点 = **菱形**（节点属性 "kind" == "decision"，
// 守卫条件走边标签，如 "[yes]"）；起止不占整框——没有显式 "initial"/"final"
// 属性时，自动把**首个人度 0 的节点**标为起点（框外左侧 `●` + 引入线）、
// **首个出度 0 的节点**标为终点（框外右侧 `◎`），标记方案与 state_machine 一致。
//
// 模型约定（沿用 graph::Graph）：Node::label() 是动作/判定名；节点属性
// "kind" == "decision" 判定，其余为动作；"initial"/"final" == "1" 显式指定起止。
#ifndef TERMCHART_GRAPH_DIAGRAMS_UML_ACTIVITY_HPP
#define TERMCHART_GRAPH_DIAGRAMS_UML_ACTIVITY_HPP

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

struct UmlActivityOptions {
    LayoutOptions layout{};
    RouteOptions route{};
    RouteStyle route_style = RouteStyle::Orthogonal;
    Style node_style{};      // 动作框
    Style decision_style{};  // 判定菱形
    Style edge_style{};      // 连线、箭头、守卫标签与起止标记
    bool mark_start_end = true;
};

// 就地布局并绘制活动图，返回整图包围盒。空图返回零矩形。
// 自动起止标记会把 "initial"/"final" 属性写回图（显式指定过则不动）。
[[nodiscard]] TERMCHART_API Rect render_uml_activity(Graph& graph, canvas::Canvas& canvas,
                                                     const UmlActivityOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_UML_ACTIVITY_HPP
