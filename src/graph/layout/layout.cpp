// layout.cpp —— 网格布局与分层布局实现
#include "termchart/graph/layout/layout.hpp"

#include <algorithm>
#include <cstddef>
#include <queue>
#include <unordered_map>
#include <vector>

#include "termchart/util/wcwidth.hpp"

namespace termchart::graph {

namespace {

// 所有节点外框的并集；无节点返回默认空矩形。
Rect union_of_boxes(const Graph& graph) noexcept {
    if (graph.nodes().empty()) {
        return Rect{};
    }
    double min_x = graph.nodes().front().box().x;
    double min_y = graph.nodes().front().box().y;
    double max_x = graph.nodes().front().box().right();
    double max_y = graph.nodes().front().box().bottom();
    for (const Node& n : graph.nodes()) {
        min_x = std::min(min_x, n.box().x);
        min_y = std::min(min_y, n.box().y);
        max_x = std::max(max_x, n.box().right());
        max_y = std::max(max_y, n.box().bottom());
    }
    return Rect{min_x, min_y, max_x - min_x, max_y - min_y};
}

// id -> 节点下标；边只可能在两端都存在时存在，故查找必定成功。
std::unordered_map<NodeId, std::size_t> index_by_id(const Graph& graph) {
    std::unordered_map<NodeId, std::size_t> index;
    index.reserve(graph.node_count());
    for (std::size_t i = 0; i < graph.nodes().size(); ++i) {
        index[graph.nodes()[i].id()] = i;
    }
    return index;
}

}  // namespace

Rect node_box_for(const Node& node, const LayoutOptions& opts) noexcept {
    const int label_width = util::display_width(node.label());
    const double width = std::max(opts.min_width, static_cast<double>(label_width) + 2.0);
    const double height = std::max(opts.min_height, 2.0);
    return Rect{0.0, 0.0, width, height};
}

Rect layout_grid(Graph& graph, std::uint32_t cols, const LayoutOptions& opts) {
    if (graph.nodes().empty()) {
        return Rect{};
    }
    if (cols == 0) {
        cols = 1;
    }

    // 先量出统一的行列尺寸，保证网格整齐。
    double cell_w = opts.min_width;
    double cell_h = opts.min_height;
    for (const Node& n : graph.nodes()) {
        const Rect size = node_box_for(n, opts);
        cell_w = std::max(cell_w, size.w);
        cell_h = std::max(cell_h, size.h);
    }

    for (std::size_t i = 0; i < graph.nodes().size(); ++i) {
        const auto col = static_cast<std::uint32_t>(i % cols);
        const auto row = static_cast<std::uint32_t>(i / cols);
        const Rect size = node_box_for(graph.nodes()[i], opts);
        const double x = opts.margin + static_cast<double>(col) * (cell_w + opts.h_gap);
        const double y = opts.margin + static_cast<double>(row) * (cell_h + opts.v_gap);
        // 节点在自家格内居中对齐，避免标签长短不一导致视觉错位。
        graph.nodes()[i].set_box(Rect{x + (cell_w - size.w) / 2.0,
                                      y + (cell_h - size.h) / 2.0, size.w, size.h});
    }
    return union_of_boxes(graph);
}

Rect layout_layered(Graph& graph, const LayoutOptions& opts) {
    if (graph.nodes().empty()) {
        return Rect{};
    }
    const std::size_t count = graph.nodes().size();
    const std::unordered_map<NodeId, std::size_t> index = index_by_id(graph);

    std::vector<std::vector<std::size_t>> out_edges(count);
    std::vector<int> in_degree(count, 0);
    for (const Edge& e : graph.edges()) {
        const auto from_it = index.find(e.from());
        const auto to_it = index.find(e.to());
        if (from_it == index.end() || to_it == index.end()) {
            continue;
        }
        out_edges[from_it->second].push_back(to_it->second);
        ++in_degree[to_it->second];
    }

    // 最长路径分层（Kahn 拓扑序）。环上的节点拿不到层号，留在 0 层。
    std::vector<int> rank(count, 0);
    std::vector<int> remaining = in_degree;
    std::queue<std::size_t> ready;
    for (std::size_t i = 0; i < count; ++i) {
        if (remaining[i] == 0) {
            ready.push(i);
        }
    }
    while (!ready.empty()) {
        const std::size_t u = ready.front();
        ready.pop();
        for (const std::size_t v : out_edges[u]) {
            rank[v] = std::max(rank[v], rank[u] + 1);
            if (--remaining[v] == 0) {
                ready.push(v);
            }
        }
    }

    int max_rank = 0;
    for (const int r : rank) {
        max_rank = std::max(max_rank, r);
    }
    const auto column_count = static_cast<std::size_t>(max_rank) + 1;

    // 每列宽度取该列最宽节点，列内高度累加。
    std::vector<double> column_width(column_count, opts.min_width);
    for (std::size_t i = 0; i < count; ++i) {
        const Rect size = node_box_for(graph.nodes()[i], opts);
        auto& width = column_width[static_cast<std::size_t>(rank[i])];
        width = std::max(width, size.w);
    }
    std::vector<double> column_x(column_count, opts.margin);
    for (std::size_t c = 1; c < column_count; ++c) {
        column_x[c] = column_x[c - 1] + column_width[c - 1] + opts.h_gap;
    }

    std::vector<double> column_y(column_count, opts.margin);
    for (std::size_t i = 0; i < count; ++i) {
        const auto column = static_cast<std::size_t>(rank[i]);
        const Rect size = node_box_for(graph.nodes()[i], opts);
        graph.nodes()[i].set_box(
            Rect{column_x[column], column_y[column], size.w, size.h});
        column_y[column] += size.h + opts.v_gap;
    }
    return union_of_boxes(graph);
}

}  // namespace termchart::graph
