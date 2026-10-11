// flowchart.cpp —— 流程图渲染：分层布局 + 共享渲染骨架
#include "termchart/graph/diagrams/flowchart.hpp"

#include "internal.hpp"

namespace termchart::graph {

Rect render_flowchart(Graph& graph, canvas::Canvas& canvas,
                      const FlowchartOptions& opts) {
    if (graph.nodes().empty()) {
        return Rect{};
    }
    const Rect bounds = layout_layered(graph, opts.layout);

    detail::DiagramStyle style;
    style.route = opts.route;
    style.route_style = opts.route_style;
    style.shape = opts.shape;
    style.node_style = opts.node_style;
    style.edge_style = opts.edge_style;
    style.node_border = true;
    style.arrows = true;
    style.edge_labels = false;
    detail::draw_diagram(graph, canvas, style);
    return bounds;
}

}  // namespace termchart::graph
