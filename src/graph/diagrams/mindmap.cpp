// mindmap.cpp —— 节点-连线视图与思维导图
#include "termchart/graph/diagrams/mindmap.hpp"

#include <cmath>
#include <map>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/graph/port.hpp"
#include "internal.hpp"

namespace termchart::graph {

namespace {

Point center_of(const Rect& box) noexcept {
    return Point{box.x + box.w / 2.0, box.y + box.h / 2.0};
}

// 弧形连线：父节点一侧伸出，拐点按**子节点登记序号**逐条外移，再横向进入子节点。
// 于是同一父节点的分支不会挤在同一条干线上（那是"梯子"），而是嵌套成扇形——
// 这才是思维导图的样子。与正交路由的差别是允许复用格子，因此不走 WireGrid。
void route_fan(Graph& graph) {
    std::map<NodeId, int> child_index;
    for (Edge& edge : graph.edges()) {
        const Node* from = graph.find(edge.from());
        const Node* to = graph.find(edge.to());
        if (from == nullptr || to == nullptr) {
            continue;
        }
        const int index = child_index[edge.from()]++;
        const double offset = static_cast<double>(index) + 1.0;
        const PortSide leave = side_towards(center_of(from->box()), center_of(to->box()));
        const Point a = port_anchor(from->box(), leave);
        const Point b = port_anchor(to->box(), opposite(leave));
        if (leave == PortSide::East || leave == PortSide::West) {
            const double elbow_x = a.x + (leave == PortSide::East ? offset : -offset);
            edge.set_route({a, Point{elbow_x, a.y}, Point{elbow_x, b.y}, b});
        } else {
            const double elbow_y = a.y + (leave == PortSide::South ? offset : -offset);
            edge.set_route({a, Point{a.x, elbow_y}, Point{b.x, elbow_y}, b});
        }
    }
}

}  // namespace

void render_node_link(Graph& graph, canvas::Canvas& canvas,
                      const NodeLinkOptions& opts) {
    detail::DiagramStyle style;
    style.route = opts.route;
    style.route_style = opts.route_style;
    style.shape = ShapeKind::Box;
    style.node_style = opts.node_style;
    style.edge_style = opts.edge_style;
    style.node_border = opts.node_border;
    style.arrows = opts.arrows;
    style.edge_labels = opts.edge_labels;
    detail::draw_diagram(graph, canvas, style);
}

Rect render_mindmap(Graph& graph, canvas::Canvas& canvas, const MindmapOptions& opts) {
    if (graph.nodes().empty()) {
        return Rect{};
    }
    const Rect bounds = layout_tree(graph, opts.tree);
    route_fan(graph);  // 扇形连线：同父分支按序号错开拐点

    detail::DiagramStyle style;
    style.shape = opts.shape;
    style.node_style = opts.topic_style;
    style.edge_style = opts.branch_style;
    style.node_border = opts.node_border;
    style.arrows = false;      // 思维导图分支不带箭头
    style.edge_labels = false;
    style.auto_route = false;  // 折线已写好，别再路由一遍
    detail::draw_diagram(graph, canvas, style);
    return bounds;
}

}  // namespace termchart::graph
