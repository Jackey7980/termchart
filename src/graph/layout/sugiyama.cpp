// sugiyama.cpp —— 四步分层布局（Sugiyama 框架）
//
// 第一步（分层）与第三步（坐标分配）复用 layout_layered 的算法骨架；本文件的核心
// 增量是第二步：用 median 启发式对每层节点重新排序，使跨层边的交叉数下降。
// 多层双向扫描（down-sweep 用上层 median，up-sweep 用下层 median）是经典的
// barycenter/median 降交叉法，O(iterations * layers * nodes log nodes)。
#include "termchart/graph/layout/sugiyama.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <queue>
#include <unordered_map>
#include <vector>

namespace termchart::graph {
namespace {

// 最长路径分层 + 破环，与 layout_layered 同算法，但把 rank 与 layers 暴露出来
// 供降交叉使用。
std::vector<int> longest_path_ranks(const Graph& graph,
                                   std::vector<std::vector<std::size_t>>& layers) {
    const std::size_t count = graph.node_count();
    std::unordered_map<NodeId, std::size_t> index;
    index.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        index[graph.nodes()[i].id()] = i;
    }
    std::vector<std::vector<std::size_t>> out_edges(count), in_edges(count);
    std::vector<int> in_deg(count, 0);
    for (const Edge& e : graph.edges()) {
        const auto fi = index.find(e.from());
        const auto ti = index.find(e.to());
        if (fi == index.end() || ti == index.end()) {
            continue;
        }
        out_edges[fi->second].push_back(ti->second);
        in_edges[ti->second].push_back(fi->second);
        ++in_deg[ti->second];
    }

    std::vector<int> rank(count, 0);
    std::vector<int> remaining = in_deg;
    std::vector<bool> processed(count, false);
    std::queue<std::size_t> ready;
    bool has_source = false;
    for (std::size_t i = 0; i < count; ++i) {
        if (remaining[i] == 0) {
            ready.push(i);
            has_source = true;
        }
    }
    if (!has_source) {
        std::size_t seed = 0;
        for (std::size_t i = 1; i < count; ++i) {
            if (remaining[i] < remaining[seed]) {
                seed = i;
            }
        }
        ready.push(seed);
        remaining[seed] = 0;
    }
    while (!ready.empty()) {
        const std::size_t u = ready.front();
        ready.pop();
        if (processed[u]) {
            continue;
        }
        processed[u] = true;
        for (const std::size_t v : out_edges[u]) {
            if (!processed[v]) {
                rank[v] = std::max(rank[v], rank[u] + 1);
            }
            if (--remaining[v] == 0 && !processed[v]) {
                ready.push(v);
            }
        }
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (processed[i]) {
            continue;
        }
        int best = -1;
        for (const std::size_t p : in_edges[i]) {
            if (processed[p]) {
                best = std::max(best, rank[p]);
            }
        }
        rank[i] = best >= 0 ? best + 1 : 0;
    }

    int max_rank = 0;
    for (const int r : rank) {
        max_rank = std::max(max_rank, r);
    }
    layers.assign(static_cast<std::size_t>(max_rank) + 1, {});
    for (std::size_t i = 0; i < count; ++i) {
        layers[static_cast<std::size_t>(rank[i])].push_back(i);
    }
    return rank;
}

// 节点 u 在相邻层邻居位置的"中位数"（median）；无邻居返回当前位置，保证稳定排序。
double median_neighbor(const std::vector<std::size_t>& neighbors,
                       const std::unordered_map<std::size_t, std::size_t>& pos,
                       double fallback) {
    if (neighbors.empty()) {
        return fallback;
    }
    std::vector<double> vals;
    vals.reserve(neighbors.size());
    for (const std::size_t n : neighbors) {
        const auto it = pos.find(n);
        if (it != pos.end()) {
            vals.push_back(static_cast<double>(it->second));
        }
    }
    if (vals.empty()) {
        return fallback;
    }
    std::sort(vals.begin(), vals.end());
    const std::size_t m = vals.size();
    if (m % 2 == 1) {
        return vals[m / 2];
    }
    return (vals[m / 2 - 1] + vals[m / 2]) / 2.0;
}

}  // namespace

Rect layout_sugiyama(Graph& graph, const LayoutOptions& opts,
                     std::uint32_t iterations) {
    if (graph.nodes().empty()) {
        return Rect{};
    }
    const std::size_t count = graph.node_count();
    std::vector<std::vector<std::size_t>> layers;
    const std::vector<int> rank = longest_path_ranks(graph, layers);
    const std::size_t layer_count = layers.size();

    // 预计算每条跨层边的两端（同层边不参与降交叉）。
    std::unordered_map<NodeId, std::size_t> index;
    index.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        index[graph.nodes()[i].id()] = i;
    }
    std::vector<std::vector<std::size_t>> up_neighbors(count);    // 指向层 l-1
    std::vector<std::vector<std::size_t>> down_neighbors(count);  // 指向层 l+1
    for (const Edge& e : graph.edges()) {
        const auto fi = index.find(e.from());
        const auto ti = index.find(e.to());
        if (fi == index.end() || ti == index.end()) {
            continue;
        }
        const int a = rank[fi->second];
        const int b = rank[ti->second];
        if (a < b) {
            down_neighbors[fi->second].push_back(ti->second);
            up_neighbors[ti->second].push_back(fi->second);
        } else if (b < a) {
            down_neighbors[ti->second].push_back(fi->second);
            up_neighbors[fi->second].push_back(ti->second);
        }
    }

    // 每层节点在当前顺序下的位置索引。
    std::vector<std::unordered_map<std::size_t, std::size_t>> pos(layer_count);
    for (std::size_t l = 0; l < layer_count; ++l) {
        for (std::size_t k = 0; k < layers[l].size(); ++k) {
            pos[l][layers[l][k]] = k;
        }
    }

    const std::uint32_t iters = std::max<std::uint32_t>(iterations, 1);
    for (std::uint32_t it = 0; it < iters; ++it) {
        if (it % 2 == 0) {  // down-sweep：用上层 median 排本层
            for (std::size_t l = 1; l < layer_count; ++l) {
                std::sort(layers[l].begin(), layers[l].end(),
                          [&](std::size_t a, std::size_t b) {
                              const double ma =
                                  median_neighbor(up_neighbors[a], pos[l - 1],
                                                  static_cast<double>(pos[l - 1].size()));
                              const double mb =
                                  median_neighbor(up_neighbors[b], pos[l - 1],
                                                  static_cast<double>(pos[l - 1].size()));
                              if (ma != mb) {
                                  return ma < mb;
                              }
                              return a < b;
                          });
            }
        } else {  // up-sweep：用下层 median 排本层；最后一层无下层，跳过
            if (layer_count >= 2) {
                for (std::size_t l = layer_count - 2; l > 0; --l) {
                    std::sort(layers[l].begin(), layers[l].end(),
                              [&](std::size_t a, std::size_t b) {
                                  const double ma =
                                      median_neighbor(down_neighbors[a], pos[l + 1],
                                                      static_cast<double>(pos[l + 1].size()));
                                  const double mb =
                                      median_neighbor(down_neighbors[b], pos[l + 1],
                                                      static_cast<double>(pos[l + 1].size()));
                                  if (ma != mb) {
                                      return ma < mb;
                                  }
                                  return a < b;
                              });
                }
            }
        }
        for (std::size_t l = 0; l < layer_count; ++l) {
            pos[l].clear();
            for (std::size_t k = 0; k < layers[l].size(); ++k) {
                pos[l][layers[l][k]] = k;
            }
        }
    }

    // 第三步：坐标分配。列 = rank，层内顺序 = 降交叉后的顺序，纵向累加。
    std::vector<double> column_width(layer_count, opts.min_width);
    for (std::size_t i = 0; i < count; ++i) {
        const Rect size = node_box_for(graph.nodes()[i], opts);
        auto& width = column_width[static_cast<std::size_t>(rank[i])];
        width = std::max(width, size.w);
    }
    std::vector<double> column_x(layer_count, opts.margin);
    for (std::size_t c = 1; c < layer_count; ++c) {
        column_x[c] = column_x[c - 1] + column_width[c - 1] + opts.h_gap;
    }

    std::vector<double> column_y(layer_count, opts.margin);
    for (std::size_t l = 0; l < layer_count; ++l) {
        for (std::size_t k = 0; k < layers[l].size(); ++k) {
            const std::size_t i = layers[l][k];
            const Rect size = node_box_for(graph.nodes()[i], opts);
            graph.nodes()[i].set_box(
                Rect{column_x[l], column_y[l], size.w, size.h});
            column_y[l] += size.h + opts.v_gap;
        }
    }

    // 所有节点外框的并集。
    Rect all = graph.nodes().front().box();
    for (std::size_t i = 1; i < count; ++i) {
        const Rect b = graph.nodes()[i].box();
        const double min_x = std::min(all.x, b.x);
        const double min_y = std::min(all.y, b.y);
        const double max_x = std::max(all.right(), b.right());
        const double max_y = std::max(all.bottom(), b.bottom());
        all = Rect{min_x, min_y, max_x - min_x, max_y - min_y};
    }
    return all;
}

}  // namespace termchart::graph
