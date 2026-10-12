// cluster.cpp —— 聚类布局实现（属性优先、连通分量兜底；组内网格、组间网格）
#include "termchart/graph/layout/cluster.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace termchart::graph {

namespace {

// 组内网格摆放（layout_grid 的子集版）：节点按登记顺序铺成 ceil(√n) 列，
// 返回组内所有外框的并集（局部坐标，原点在 0,0）。
Rect place_subset_grid(Graph& graph, const std::vector<std::size_t>& members,
                       const LayoutOptions& opts) {
    if (members.empty()) {
        return Rect{};
    }
    const auto cols = static_cast<std::uint32_t>(
        std::max(1.0, std::ceil(std::sqrt(static_cast<double>(members.size())))));

    double cell_w = opts.min_width;
    double cell_h = opts.min_height;
    for (const std::size_t i : members) {
        const Rect size = node_box_for(graph.nodes()[i], opts);
        cell_w = std::max(cell_w, size.w);
        cell_h = std::max(cell_h, size.h);
    }

    double union_right = 0.0;
    double union_bottom = 0.0;
    for (std::size_t k = 0; k < members.size(); ++k) {
        Node& node = graph.nodes()[members[k]];
        const Rect size = node_box_for(node, opts);
        const auto col = static_cast<std::uint32_t>(k % cols);
        const auto row = static_cast<std::uint32_t>(k / cols);
        const double x = static_cast<double>(col) * (cell_w + opts.h_gap);
        const double y = static_cast<double>(row) * (cell_h + opts.v_gap);
        // 组内同样在自家格里居中，与 layout_grid 的观感一致。
        node.set_box(Rect{x + (cell_w - size.w) / 2.0, y + (cell_h - size.h) / 2.0,
                          size.w, size.h});
        union_right = std::max(union_right, node.box().right());
        union_bottom = std::max(union_bottom, node.box().bottom());
    }
    return Rect{0.0, 0.0, union_right, union_bottom};
}

}  // namespace

Rect layout_cluster(Graph& graph, const ClusterOptions& opts) {
    const std::size_t count = graph.nodes().size();
    if (count == 0) {
        return Rect{};
    }

    // 1) 分组：属性 "cluster" 优先，其余按无向连通分量。
    std::vector<std::string> group_key(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::string_view attr = graph.nodes()[i].attributes().get("cluster");
        if (!attr.empty()) {
            group_key[i] = std::string(attr);
        }
    }
    std::vector<std::vector<std::size_t>> adj(count);
    for (const Edge& e : graph.edges()) {
        const Node* from = graph.find(e.from());
        const Node* to = graph.find(e.to());
        if (from == nullptr || to == nullptr || from == to) {
            continue;
        }
        const auto u = static_cast<std::size_t>(from - graph.nodes().data());
        const auto v = static_cast<std::size_t>(to - graph.nodes().data());
        adj[u].push_back(v);
        adj[v].push_back(u);
    }
    // 连通分量编号只发给没有显式属性的节点；显式属性即便互相连通也各自成组。
    std::vector<int> component(count, -1);
    int next_component = 0;
    for (std::size_t seed = 0; seed < count; ++seed) {
        if (!group_key[seed].empty() || component[seed] >= 0) {
            continue;
        }
        std::vector<std::size_t> stack{seed};
        component[seed] = next_component;
        while (!stack.empty()) {
            const std::size_t u = stack.back();
            stack.pop_back();
            for (const std::size_t v : adj[u]) {
                if (group_key[v].empty() && component[v] < 0) {
                    component[v] = next_component;
                    stack.push_back(v);
                }
            }
        }
        ++next_component;
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (group_key[i].empty()) {
            group_key[i] = "__component_" + std::to_string(component[i]);
        }
    }

    std::map<std::string, std::vector<std::size_t>> groups;
    for (std::size_t i = 0; i < count; ++i) {
        groups[group_key[i]].push_back(i);
    }

    // 2) 组内摆好（局部坐标），量出各组包围盒。
    struct ClusterBox {
        Rect box{};
        const std::vector<std::size_t>* members = nullptr;
    };
    std::vector<ClusterBox> clusters;
    clusters.reserve(groups.size());
    for (auto& [key, members] : groups) {
        (void)key;
        ClusterBox item;
        item.members = &members;
        item.box = place_subset_grid(graph, members, opts.layout);
        clusters.push_back(item);
    }

    // 3) 组间网格：包围盒按 ceil(√k) 列排布，随后平移到全局坐标。
    const auto cluster_cols = static_cast<std::uint32_t>(std::max(
        1.0, std::ceil(std::sqrt(static_cast<double>(clusters.size())))));
    std::vector<double> col_right(cluster_cols, 0.0);
    std::vector<double> row_bottom(1, 0.0);
    for (std::size_t k = 0; k < clusters.size(); ++k) {
        const auto col = static_cast<std::uint32_t>(k % cluster_cols);
        const auto row = static_cast<std::uint32_t>(k / cluster_cols);
        if (row_bottom.size() <= row) {
            row_bottom.resize(static_cast<std::size_t>(row) + 1, 0.0);
        }
        col_right[col] = std::max(col_right[col], clusters[k].box.right());
        row_bottom[row] = std::max(row_bottom[row], clusters[k].box.bottom());
    }
    std::vector<double> col_x(cluster_cols, opts.layout.margin);
    for (std::uint32_t c = 1; c < cluster_cols; ++c) {
        col_x[c] = col_x[c - 1] + col_right[c - 1] + opts.cluster_gap;
    }
    std::vector<double> row_y(row_bottom.size(), opts.layout.margin);
    for (std::size_t r = 1; r < row_y.size(); ++r) {
        row_y[r] = row_y[r - 1] + row_bottom[r - 1] + opts.cluster_gap;
    }

    for (std::size_t k = 0; k < clusters.size(); ++k) {
        const auto col = static_cast<std::uint32_t>(k % cluster_cols);
        const auto row = static_cast<std::uint32_t>(k / cluster_cols);
        const double dx = col_x[col] - clusters[k].box.x;
        const double dy = row_y[row] - clusters[k].box.y;
        for (const std::size_t i : *clusters[k].members) {
            Node& node = graph.nodes()[i];
            const Rect b = node.box();
            node.set_box(Rect{b.x + dx, b.y + dy, b.w, b.h});
        }
    }
    return graph.bounds();
}

}  // namespace termchart::graph
