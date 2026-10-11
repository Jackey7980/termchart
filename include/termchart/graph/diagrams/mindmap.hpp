// mindmap.hpp —— 思维导图 + 通用"节点-连线"视图
//
// render_node_link 是**不做布局**的通用渲染：调用方先自行布局（layout_force /
// layout_grid / ...），它只负责把节点与边画出来。放在本头文件，是因为思维导图
// 正是这类视图的典型形态；力导向图（`examples/20_force_graph`）复用它。
//
// render_mindmap = layout_tree(left_right) + render_node_link（默认不画边框，
// 标签直接作为节点，连线即分支）。
#ifndef TERMCHART_GRAPH_DIAGRAMS_MINDMAP_HPP
#define TERMCHART_GRAPH_DIAGRAMS_MINDMAP_HPP

#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/tree_layout.hpp"
#include "termchart/graph/route/route.hpp"
#include "termchart/graph/shape/shape.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct NodeLinkOptions {
    RouteOptions route{};
    RouteStyle route_style = RouteStyle::Straight;
    Style node_style{};
    Style edge_style{};
    bool node_border = false;
    bool arrows = false;
    bool edge_labels = false;
};

// 通用节点-连线渲染（不做布局，调用方先布局）。
TERMCHART_API void render_node_link(Graph& graph, canvas::Canvas& canvas,
                                    const NodeLinkOptions& opts = {});

struct MindmapOptions {
    TreeOptions tree;       // 默认 left_right = true（根居中，分支分列两侧）
    ShapeKind shape = ShapeKind::Box;
    Style topic_style{};    // 标签样式
    Style branch_style{};   // 分支连线样式
    bool node_border = false;

    MindmapOptions() { tree.left_right = true; }
};

// 就地布局并绘制思维导图，返回整图包围盒。
TERMCHART_API Rect render_mindmap(Graph& graph, canvas::Canvas& canvas,
                                  const MindmapOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_MINDMAP_HPP
