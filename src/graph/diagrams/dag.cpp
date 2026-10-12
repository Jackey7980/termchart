// dag.cpp —— DAG 渲染实现（环检测 + 分层布局 + 共享渲染骨架）
#include "termchart/graph/diagrams/dag.hpp"

#include <cstddef>
#include <queue>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "internal.hpp"

namespace termchart::graph {

bool is_acyclic(const Graph& graph) {
    const std::size_t count = graph.nodes().size();
    std::vector<std::size_t> in_degree(count, 0);
    std::vector<std::vector<std::size_t>> out_edges(count);
    for (const Edge& e : graph.edges()) {
        const Node* from = graph.find(e.from());
        const Node* to = graph.find(e.to());
        if (from == nullptr || to == nullptr) {
            continue;
        }
        const auto u = static_cast<std::size_t>(from - graph.nodes().data());
        const auto v = static_cast<std::size_t>(to - graph.nodes().data());
        out_edges[u].push_back(v);
        ++in_degree[v];  // 自环也会把自己堵死，从而被判成环
    }
    std::queue<std::size_t> ready;
    for (std::size_t i = 0; i < count; ++i) {
        if (in_degree[i] == 0) {
            ready.push(i);
        }
    }
    std::size_t processed = 0;
    while (!ready.empty()) {
        const std::size_t u = ready.front();
        ready.pop();
        ++processed;
        for (const std::size_t v : out_edges[u]) {
            if (--in_degree[v] == 0) {
                ready.push(v);
            }
        }
    }
    return processed == count;
}

Rect render_dag(Graph& graph, canvas::Canvas& canvas, const DagOptions& opts) {
    if (graph.nodes().empty()) {
        return Rect{};
    }
    // 有环时 layout_layered 自带破环（环上节点挂到已定层前驱的下一层），
    // 这里不做特殊处理：不报错、不丢边，环的回边照常画出。
    const Rect bounds = layout_layered(graph, opts.layout);

    detail::DiagramStyle style;
    style.route = opts.route;
    style.route_style = RouteStyle::Orthogonal;
    style.shape = opts.shape;
    style.node_style = opts.node_style;
    style.edge_style = opts.edge_style;
    style.node_border = true;
    style.arrows = opts.arrows;
    style.edge_labels = opts.edge_labels;
    detail::draw_diagram(graph, canvas, style);
    return bounds;
}

}  // namespace termchart::graph