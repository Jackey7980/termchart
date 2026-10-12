// knowledge_graph.cpp —— 知识图谱渲染：力导向布局 + 节点-连线视图
#include "termchart/graph/diagrams/knowledge_graph.hpp"

#include "termchart/canvas/canvas.hpp"
#include "termchart/graph/diagrams/mindmap.hpp"

namespace termchart::graph {

Rect render_knowledge_graph(Graph& graph, canvas::Canvas& canvas,
                            const KnowledgeGraphOptions& opts) {
    if (graph.nodes().empty()) {
        return Rect{};
    }
    const Rect bounds = layout_force(graph, opts.force);

    NodeLinkOptions link;
    link.route = opts.route;
    link.route_style = opts.route_style;
    link.node_style = opts.node_style;
    link.edge_style = opts.edge_style;
    link.node_border = opts.node_border;
    link.arrows = opts.arrows;
    link.edge_labels = opts.edge_labels;
    render_node_link(graph, canvas, link);
    return bounds;
}

}  // namespace termchart::graph
