// knowledge_graph.hpp —— 知识图谱（实体-关系网络）
//
// 字符化策略：实体画带边框的盒子并居中标名，关系画直线连线（可带箭头与
// 关系标签）；布局复用力导向（layout_force），绘制复用节点-连线视图
// （render_node_link）。本层只做"布局 + 视图"的选项组合，不新增绘制逻辑。
#ifndef TERMCHART_GRAPH_DIAGRAMS_KNOWLEDGE_GRAPH_HPP
#define TERMCHART_GRAPH_DIAGRAMS_KNOWLEDGE_GRAPH_HPP

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/force.hpp"
#include "termchart/graph/route/route.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct KnowledgeGraphOptions {
    ForceOptions force{};   // 力导向布局参数
    RouteOptions route{};
    RouteStyle route_style = RouteStyle::Straight;
    Style node_style{};
    Style edge_style{};
    bool node_border = true;
    bool arrows = false;       // 知识图谱关系方向常弱于结构，默认不画箭头
    bool edge_labels = false;  // 关系名（复用 label_place 避让节点框）
};

// 就地力导向布局并绘制知识图谱，返回整图包围盒。
TERMCHART_API Rect render_knowledge_graph(Graph& graph, canvas::Canvas& canvas,
                                          const KnowledgeGraphOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_KNOWLEDGE_GRAPH_HPP
