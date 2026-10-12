// dependency.cpp —— 依赖图渲染实现（环边标记 + 复用 dag 渲染）
#include "termchart/graph/diagrams/dependency.hpp"

#include <cstddef>
#include <vector>

#include "termchart/canvas/canvas.hpp"

namespace termchart::graph {

namespace {

// 反向可达：from 能沿边走到 to（含 from == to，用于自环判定）。
bool reachable(std::size_t from, std::size_t to,
               const std::vector<std::vector<std::size_t>>& out_edges) {
    if (from == to) {
        return true;
    }
    std::vector<char> seen(out_edges.size(), 0);
    std::vector<std::size_t> stack{from};
    seen[from] = 1;
    while (!stack.empty()) {
        const std::size_t u = stack.back();
        stack.pop_back();
        for (const std::size_t v : out_edges[u]) {
            if (v == to) {
                return true;
            }
            if (seen[v] == 0) {
                seen[v] = 1;
                stack.push_back(v);
            }
        }
    }
    return false;
}

}  // namespace

Rect render_dependency(Graph& graph, canvas::Canvas& canvas,
                       const DependencyOptions& opts) {
    if (graph.nodes().empty()) {
        return Rect{};
    }

    if (opts.mark_cycles) {
        // 边 (u,v) 在环上 ⇔ 存在 v→u 的路径（自环天然成立）。
        const std::size_t count = graph.nodes().size();
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
        }
        for (Edge& e : graph.edges()) {
            const Node* from = graph.find(e.from());
            const Node* to = graph.find(e.to());
            if (from == nullptr || to == nullptr) {
                continue;
            }
            const auto u = static_cast<std::size_t>(from - graph.nodes().data());
            const auto v = static_cast<std::size_t>(to - graph.nodes().data());
            if (reachable(v, u, out_edges) && e.arrow() == ArrowStyle::AtTarget) {
                e.set_arrow(ArrowStyle::Both);
            }
        }
    }

    return render_dag(graph, canvas, opts.dag);
}

}  // namespace termchart::graph