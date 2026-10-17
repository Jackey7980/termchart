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
#include <limits>
#include <queue>
#include <unordered_map>
#include <vector>

#include "cycle_removal.hpp"
#include "network_simplex.hpp"

namespace termchart::graph {
namespace {

using layout_internal::DirectedEdge;

// 把 rank 表组装成"层 → 节点下标"的分桶。
std::vector<std::vector<std::size_t>> layers_from_ranks(const std::vector<int>& rank) {
    int max_rank = 0;
    for (const int r : rank) {
        max_rank = std::max(max_rank, r);
    }
    std::vector<std::vector<std::size_t>> layers(static_cast<std::size_t>(max_rank) + 1);
    for (std::size_t i = 0; i < rank.size(); ++i) {
        layers[static_cast<std::size_t>(rank[i])].push_back(i);
    }
    return layers;
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

// 序列逆序对数（Fenwick 树，O(n log n)）：用于精确统计相邻层交叉。
std::size_t inversion_count(const std::vector<std::size_t>& seq, std::size_t max_value) {
    std::vector<std::size_t> bit(max_value + 2, 0);
    std::size_t inv = 0;
    for (std::size_t i = 0; i < seq.size(); ++i) {
        const std::size_t v = seq[i] + 1;  // Fenwick 下标从 1 起
        std::size_t le = 0;                // 已出现且位置 <= 当前 的边数
        for (std::size_t k = v; k > 0; k -= k & (0u - k)) {
            le += bit[k];
        }
        inv += i - le;  // 之前出现、位置更大的边与当前边交叉
        for (std::size_t k = v; k < bit.size(); k += k & (0u - k)) {
            ++bit[k];
        }
    }
    return inv;
}

// 相邻层 l / l+1 之间的精确交叉数：边按"上层位置、下层位置"排序后，下层位置
// 序列的逆序对数即交叉数。只统计 rank 差恰为 1 的边——跨多层的长边不进相邻层
// 的交叉口径（视觉上它穿过中间层，但降交叉只按相邻层计）。
std::size_t bilayer_crossings(const std::vector<std::vector<std::size_t>>& layers,
                              const std::vector<std::unordered_map<std::size_t, std::size_t>>& pos,
                              const std::vector<std::vector<std::size_t>>& up_neighbors,
                              std::size_t l) {
    if (l + 1 >= layers.size()) {
        return 0;
    }
    std::vector<std::pair<std::size_t, std::size_t>> pairs;
    for (const std::size_t v : layers[l + 1]) {
        const auto pv = pos[l + 1].find(v);
        if (pv == pos[l + 1].end()) {
            continue;
        }
        for (const std::size_t u : up_neighbors[v]) {
            const auto pu = pos[l].find(u);  // 不在 l 层 ⇒ 长边，跳过
            if (pu == pos[l].end()) {
                continue;
            }
            pairs.emplace_back(pu->second, pv->second);
        }
    }
    if (pairs.size() < 2) {
        return 0;
    }
    std::sort(pairs.begin(), pairs.end());  // 同起点时按对端升序 ⇒ 不误计为交叉
    std::vector<std::size_t> seq;
    seq.reserve(pairs.size());
    for (const auto& pair : pairs) {
        seq.push_back(pair.second);
    }
    return inversion_count(seq, layers[l + 1].size());
}

// 连通分量（无向 BFS，忽略悬空端点/自环）。分量内节点按登记顺序，保证确定性。
std::vector<std::vector<std::size_t>> connected_components_local(
    std::size_t count, const std::unordered_map<NodeId, std::size_t>& index, const Graph& graph) {
    std::vector<std::vector<std::size_t>> adj(count);
    for (const Edge& e : graph.edges()) {
        const auto fi = index.find(e.from());
        const auto ti = index.find(e.to());
        if (fi == index.end() || ti == index.end()) {
            continue;  // 悬空端点
        }
        if (fi->second == ti->second) {
            continue;  // 自环
        }
        adj[fi->second].push_back(ti->second);
        adj[ti->second].push_back(fi->second);
    }
    std::vector<int> comp(count, -1);
    std::vector<std::vector<std::size_t>> components;
    for (std::size_t s = 0; s < count; ++s) {
        if (comp[s] >= 0) {
            continue;
        }
        const int id = static_cast<int>(components.size());
        components.emplace_back();
        std::vector<std::size_t> queue{s};
        comp[s] = id;
        for (std::size_t head = 0; head < queue.size(); ++head) {
            components.back().push_back(queue[head]);
            for (const std::size_t nb : adj[queue[head]]) {
                if (comp[nb] < 0) {
                    comp[nb] = id;
                    queue.push_back(nb);
                }
            }
        }
    }
    return components;
}

}  // namespace

Rect layout_sugiyama(Graph& graph, const LayoutOptions& opts,
                     std::uint32_t iterations) {
    if (graph.nodes().empty()) {
        return Rect{};
    }
    const std::size_t count = graph.node_count();
    // id → 下标：后续所有表按下标组织。
    std::unordered_map<NodeId, std::size_t> index;
    index.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        index[graph.nodes()[i].id()] = i;
    }

    // —— 第一步：破环（回边反转）→ 最长路径分层 ——
    // 分层要求无环。以前是"Kahn 走不动了就地兜底"，含环图上那套兜底会把环所在的
    // 整条链塌到第 0 层（60 示例 --cycle 实测 L0 塞进 17 个节点）。现在先打成 DAG，
    // 分层只面对无环图，就不需要兜底了。
    std::vector<std::pair<std::size_t, std::size_t>> raw_edges;
    raw_edges.reserve(graph.edge_count());
    for (const Edge& e : graph.edges()) {
        const auto fi = index.find(e.from());
        const auto ti = index.find(e.to());
        if (fi == index.end() || ti == index.end()) {
            continue;  // 悬空端点的边：分层直接忽略
        }
        raw_edges.emplace_back(fi->second, ti->second);
    }
    const layout_internal::CycleRemoval dag = layout_internal::remove_cycles(count, raw_edges);

    // 分层：network simplex（Gansner et al. 1993）最小化边总长，而不是最长路径
    // 那种"全压到第 0 层"的可行解。挂在链中段的叶子会被 NS 挪到它该在的层，
    // 边总长更短 → dummy 更少、图更矮。
    //
    // **反转的回边不参与 rank 优化**：它本来的方向是 G→A（视觉上就该往回走），
    // 反转只是为了让图可分层。若把它当普通边塞进 NS，那条跨 6 层的"伪长边"会
    // 诱惑 NS 把源点 A0 抬 5 层去缩短它——实测含环图 L0 只剩 2 个节点、A 层全被
    // 压到 L1，交叉数反而从 70 涨到 80。回边的 rank 由其余边决定，它自己只按
    // 最终 rank 差插 dummy。
    std::vector<DirectedEdge> ns_edges;
    ns_edges.reserve(dag.edges.size());
    for (const DirectedEdge& e : dag.edges) {
        if (!e.reversed) {
            ns_edges.push_back(e);
        }
    }
    const std::vector<int> rank = layout_internal::rank_assignment_ns(count, ns_edges);
    std::vector<std::vector<std::size_t>> layers = layers_from_ranks(rank);
    const std::size_t layer_count = layers.size();

    // —— 第一步半：跨层边插入 dummy（不进 Graph）——
    // rank 差 > 1 的边在它跨过的每个中间列各占一个"槽位"（dummy）。作用有两个：
    //   1) 排序：长边在中间列有了话语权，不再只按端点列的先后瞎排（此前长边被
    //      当成一跳邻居参与 median，拿到的位置信息本身就是失真的）；
    //   2) 走线：长边穿过中间列时走 dummy 所在的槽位行——那一行没有实节点框，
    //      竖直段落在列与列之间的通道列，整条路径真正正交且不穿框。
    // dummy 用下标 >= count 表示，只活在下面的表里：Graph 的 node_count()/edges()
    // 是用户数据，往里插节点会改变对外可见的图。
    struct LongEdge {
        std::size_t edge = 0;              // graph.edges() 下标
        std::size_t from = 0;              // 边源在 graph.nodes() 里的下标
        std::size_t to = 0;                // 边目标下标
        bool source_left = true;           // 源列在目标列左侧
        std::vector<std::size_t> dummies;  // 沿 from → to 依次经过的 dummy
    };
    std::vector<LongEdge> long_edges;
    std::vector<std::size_t> dummy_column;  // dummy 下标 - count → 所在列

    std::vector<std::vector<std::size_t>> up_neighbors(count);    // 指向层 l-1
    std::vector<std::vector<std::size_t>> down_neighbors(count);  // 指向层 l+1
    const auto ensure_neighbor_slots = [&](std::size_t n) {
        if (up_neighbors.size() <= n) {
            up_neighbors.resize(n + 1);
            down_neighbors.resize(n + 1);
        }
    };
    for (std::size_t ei = 0; ei < graph.edges().size(); ++ei) {
        const Edge& e = graph.edges()[ei];
        const auto fi = index.find(e.from());
        const auto ti = index.find(e.to());
        if (fi == index.end() || ti == index.end()) {
            continue;
        }
        const std::size_t u = fi->second;
        const std::size_t v = ti->second;
        const int a = rank[u];
        const int b = rank[v];
        if (a == b) {
            continue;  // 同层边不参与降交叉
        }
        const bool source_left = a < b;
        const std::size_t lo = source_left ? u : v;
        const std::size_t hi = source_left ? v : u;
        const int rlo = std::min(a, b);
        const int rhi = std::max(a, b);
        if (rhi - rlo == 1) {
            down_neighbors[lo].push_back(hi);
            up_neighbors[hi].push_back(lo);
            continue;
        }
        LongEdge le;
        le.edge = ei;
        le.from = u;
        le.to = v;
        le.source_left = source_left;
        std::size_t prev = lo;
        for (int r = rlo + 1; r < rhi; ++r) {
            const std::size_t d = count + dummy_column.size();
            ensure_neighbor_slots(d);
            dummy_column.push_back(static_cast<std::size_t>(r));
            le.dummies.push_back(d);
            down_neighbors[prev].push_back(d);
            up_neighbors[d].push_back(prev);
            prev = d;
        }
        down_neighbors[prev].push_back(hi);
        up_neighbors[hi].push_back(prev);
        long_edges.push_back(std::move(le));
    }

    // 层表扩上 dummy：排序 / 交叉统计 / 坐标分配一律走 ext_layers。
    std::vector<std::vector<std::size_t>> ext_layers = layers;
    for (std::size_t k = 0; k < dummy_column.size(); ++k) {
        ext_layers[dummy_column[k]].push_back(count + k);
    }

    // 每层节点在当前顺序下的位置索引。
    std::vector<std::unordered_map<std::size_t, std::size_t>> pos(layer_count);
    for (std::size_t l = 0; l < layer_count; ++l) {
        for (std::size_t k = 0; k < ext_layers[l].size(); ++k) {
            pos[l][ext_layers[l][k]] = k;
        }
    }

    // 重排完一层**立刻**刷新该层的位置表：同一轮 sweep 内后续层要看到最新顺序。
    // 若攒到轮末统一刷新，就出现"L2 已经翻成 G,H，而 L1 仍按旧的 H,G 重排"，
    // 两次 sweep 的效果互相抵消，偶数轮退化回登记顺序（交叉数不降）。
    const auto refresh_pos = [&](std::size_t l) {
        pos[l].clear();
        for (std::size_t k = 0; k < ext_layers[l].size(); ++k) {
            pos[l][ext_layers[l][k]] = k;
        }
    };

    const auto crossings_at = [&](std::size_t l) {
        return bilayer_crossings(ext_layers, pos, up_neighbors, l);
    };
    const auto total_crossings = [&]() {
        std::size_t sum = 0;
        for (std::size_t l = 0; l + 1 < layer_count; ++l) {
            sum += crossings_at(l);
        }
        return sum;
    };
    // 走线长度代价：所有相邻层段的"行位差"之和。pos 先按各层宽度归一化到
    // [0,1]，否则层宽不同的两列没法比。交叉数相同的两个解里选这个更小的——
    // 否则降交叉会奖励"把 dummy 甩到第 8 行换掉一个交叉"，黄线就越绕越远。
    const auto length_cost = [&]() {
        double sum = 0.0;
        for (std::size_t l = 0; l + 1 < layer_count; ++l) {
            const double span_lo =
                static_cast<double>(std::max<std::size_t>(ext_layers[l].size(), 1) - 1);
            const double span_hi =
                static_cast<double>(std::max<std::size_t>(ext_layers[l + 1].size(), 1) - 1);
            const double norm_lo = span_lo > 0.0 ? span_lo : 1.0;
            const double norm_hi = span_hi > 0.0 ? span_hi : 1.0;
            for (const std::size_t v : ext_layers[l + 1]) {
                const auto pv = pos[l + 1].find(v);
                if (pv == pos[l + 1].end()) {
                    continue;
                }
                for (const std::size_t u : up_neighbors[v]) {
                    const auto pu = pos[l].find(u);  // 不在 l 层 ⇒ 同层/跨层，跳过
                    if (pu == pos[l].end()) {
                        continue;
                    }
                    sum += std::fabs(static_cast<double>(pu->second) / norm_lo -
                                     static_cast<double>(pv->second) / norm_hi);
                }
            }
        }
        return sum;
    };

    // 判优：交叉数优先（绝不允许反弹），相等时比走线长度。等价于在"交叉数不升"
    // 的约束下把 λ 加权代价做到最小，但不用调 λ——交叉优先这条硬约束已经把
    // λ 的自由度砍掉了。
    const auto better = [](std::size_t c_new, double len_new, std::size_t c_old, double len_old) {
        if (c_new != c_old) {
            return c_new < c_old;
        }
        return len_new < len_old - 1.0e-12;
    };

    // 相邻交换（transpose）：median 只按"邻居位置的中位数"给节点定序，两个相邻
    // 节点整体挪一格往往更优却扫不出来。这里逐对试交换，按 (交叉数, 长度) 裁决。
    const auto transpose_pass = [&]() {
        bool improved = false;
        for (std::size_t l = 0; l < layer_count; ++l) {
            for (std::size_t k = 0; k + 1 < ext_layers[l].size(); ++k) {
                const std::size_t before_c = total_crossings();
                const double before_len = length_cost();
                std::swap(ext_layers[l][k], ext_layers[l][k + 1]);
                refresh_pos(l);
                if (better(total_crossings(), length_cost(), before_c, before_len)) {
                    improved = true;
                } else {
                    std::swap(ext_layers[l][k], ext_layers[l][k + 1]);  // 换回去
                    refresh_pos(l);
                }
            }
        }
        return improved;
    };

    // dummy 的 sifting：把每个 dummy 挪到层内每个位置试一遍，按同一判据取最优。
    // dummy 的度数恒为 2、数量也少（密集示例 10 个），所以全量重算交叉数/长度
    // 就够快——不必写增量版本，少一处容易算错的优化。
    const auto sift_pass = [&]() {
        bool improved = false;
        for (std::size_t l = 0; l < layer_count; ++l) {
            std::vector<std::size_t>& layer = ext_layers[l];
            for (std::size_t origin = 0; origin < layer.size(); ++origin) {
                if (layer[origin] < count) {
                    continue;  // 只动 dummy：真实节点的行位交给 median
                }
                const std::size_t value = layer[origin];
                std::size_t best_pos = origin;
                std::size_t best_c = total_crossings();
                double best_len = length_cost();
                for (std::size_t target = 0; target < layer.size(); ++target) {
                    if (target == origin) {
                        continue;
                    }
                    layer.erase(layer.begin() + static_cast<std::ptrdiff_t>(origin));
                    layer.insert(layer.begin() + static_cast<std::ptrdiff_t>(target), value);
                    refresh_pos(l);
                    const std::size_t c = total_crossings();
                    const double len = length_cost();
                    if (better(c, len, best_c, best_len)) {
                        best_c = c;
                        best_len = len;
                        best_pos = target;
                    }
                    layer.erase(layer.begin() + static_cast<std::ptrdiff_t>(target));
                    layer.insert(layer.begin() + static_cast<std::ptrdiff_t>(origin), value);
                    refresh_pos(l);
                }
                if (best_pos != origin) {
                    layer.erase(layer.begin() + static_cast<std::ptrdiff_t>(origin));
                    layer.insert(layer.begin() + static_cast<std::ptrdiff_t>(best_pos), value);
                    refresh_pos(l);
                    improved = true;
                }
            }
        }
        return improved;
    };

    // median 启发式本身会震荡（实测密集图 3 轮 67 交叉、4 轮反弹到 74），
    // 取"最后一轮"会把更差的排布当成结果，因此每轮结束按同一判据留历史最优。
    std::vector<std::vector<std::size_t>> best_layers = ext_layers;
    std::size_t best_crossings = total_crossings();
    double best_length = length_cost();

    // median 相同时用 stable_sort 保持该层当前次序（不用节点登记索引做 tie-break：
    // 那会把"上一轮排出来的顺序"重新抹平成登记顺序）。
    const std::uint32_t iters = std::max<std::uint32_t>(iterations, 1);
    for (std::uint32_t it = 0; it < iters; ++it) {
        if (it % 2 == 0) {  // down-sweep：用上层 median 排本层（第 0 层无上层，跳过）
            for (std::size_t l = 1; l < layer_count; ++l) {
                std::stable_sort(ext_layers[l].begin(), ext_layers[l].end(),
                                 [&](std::size_t a, std::size_t b) {
                                     return median_neighbor(up_neighbors[a], pos[l - 1],
                                                            static_cast<double>(pos[l - 1].size())) <
                                            median_neighbor(up_neighbors[b], pos[l - 1],
                                                            static_cast<double>(pos[l - 1].size()));
                                 });
                refresh_pos(l);
            }
        } else {  // up-sweep：用下层 median 排本层（含第 0 层；最后一层无下层，跳过）
            if (layer_count >= 2) {
                for (std::size_t l = layer_count - 2;; --l) {
                    std::stable_sort(
                        ext_layers[l].begin(), ext_layers[l].end(), [&](std::size_t a, std::size_t b) {
                            return median_neighbor(down_neighbors[a], pos[l + 1],
                                                   static_cast<double>(pos[l + 1].size())) <
                                   median_neighbor(down_neighbors[b], pos[l + 1],
                                                   static_cast<double>(pos[l + 1].size()));
                        });
                    refresh_pos(l);
                    if (l == 0) {
                        break;
                    }
                }
            }
        }
        for (std::uint32_t t = 0; t < 2; ++t) {
            if (!transpose_pass()) {
                break;
            }
        }
        const std::size_t current = total_crossings();
        const double current_len = length_cost();
        if (better(current, current_len, best_crossings, best_length)) {
            best_crossings = current;
            best_length = current_len;
            best_layers = ext_layers;
        }
    }
    ext_layers = best_layers;  // 输出历史最优，而不是最后一轮

    // median + transpose 收敛后会停在"交叉数不动"的平台上；再对 dummy 做
    // sifting，并把走线长度作为 tie-break，把长边的绕行压下来。sifting 与
    // transpose 可能交替改进，循环到都不动为止（上限 4 轮，防病态图上横跳）。
    for (std::uint32_t t = 0; t < 4; ++t) {
        bool changed = false;
        for (std::uint32_t k = 0; k < 2; ++k) {
            if (!transpose_pass()) {
                break;
            }
            changed = true;
        }
        if (sift_pass()) {
            changed = true;
        }
        if (!changed) {
            break;
        }
    }
    if (better(total_crossings(), length_cost(), best_crossings, best_length)) {
        best_crossings = total_crossings();
        best_length = length_cost();
        best_layers = ext_layers;
    }
    ext_layers = best_layers;

    // 第三步：坐标分配。列 = rank，层内顺序 = 降交叉后的顺序，纵向累加。
    std::vector<double> column_width(layer_count, opts.min_width);
    std::vector<Rect> sizes(count);
    for (std::size_t i = 0; i < count; ++i) {
        sizes[i] = node_box_for(graph.nodes()[i], opts);
        auto& width = column_width[static_cast<std::size_t>(rank[i])];
        width = std::max(width, sizes[i].w);
    }
    std::vector<double> column_x(layer_count, opts.margin);
    for (std::size_t c = 1; c < layer_count; ++c) {
        column_x[c] = column_x[c - 1] + column_width[c - 1] + opts.h_gap;
    }

    // —— 坐标分配：层内 y 对齐（优先级法的两趟 + 平衡）——
    // 之前是"顺次紧堆"：y = 上一个的底部 + v_gap。那样每个节点只跟它上面那个
    // 保持固定间距，跟邻居对不对齐没人管，父子几乎不在同一行 ⇒ 每条边都拐两次。
    // 这里改成：先按下层/上层邻居中心的中位数定目标位置，在"同层不重叠"约束下
    // 从上往下压紧（下扫），再从下往上压紧（上扫），最后取两者的中点——即
    // Brandes–Köpf 的平衡思想，先不做 block 级对齐（层内没有多节点块）。
    // dummy 与真实节点同一套表：它的 y 就是长边横穿该列时走的行。
    constexpr double kDummyHeight = 1.0;
    const std::size_t node_total = count + dummy_column.size();
    std::vector<double> heights(node_total, kDummyHeight);
    for (std::size_t i = 0; i < count; ++i) {
        heights[i] = sizes[i].h;
    }
    std::vector<double> dummy_row(dummy_column.size(), 0.0);
    std::vector<double> final_y(node_total, opts.margin);
    std::vector<double> y_down(node_total, opts.margin);  // 对齐分支：DP 结果（各行位）

    if (!opts.align_rows) {  // 关掉对齐：顺次紧堆（最紧凑）
        std::vector<double> cursor(layer_count, opts.margin);
        for (std::size_t l = 0; l < layer_count; ++l) {
            for (const std::size_t v : ext_layers[l]) {
                final_y[v] = cursor[l];
                cursor[l] += heights[v] + opts.v_gap;
            }
        }
    } else {
        // 离散吸附（不是"贴近中位数"）。对多个邻居取中位数会给出一个"谁的边
        // 都没对齐"的位置——实测直线边占比从 29% 掉到 11%。这里改成：每个节点
        // 的 y 只能从"刚好对齐某个上一层邻居"的候选值里挑，逐层自上而下做 DP，
        // 在"同层不重叠"的约束下最大化"成为一条水平线"的边数；同样对齐数时取
        // 更靠上的解（保持图紧凑）。dummy 走同一套：它的 y 就是长边横穿该列的行。
        struct State {
            int gained = -1;      // 与上一层对齐的边数（-1 = 不可达）
            double total = 0.0;   // 层内累计 y，用作同分时的紧凑度
        };
        for (std::size_t l = 0; l < layer_count; ++l) {
            const std::vector<std::size_t>& layer = ext_layers[l];
            const std::size_t n = layer.size();
            std::vector<std::vector<double>> cand(n);
            std::vector<std::vector<int>> gain(n);
            for (std::size_t i = 0; i < n; ++i) {
                const std::size_t v = layer[i];
                std::vector<double> options;
                for (const std::size_t u : up_neighbors[v]) {
                    options.push_back(y_down[u] + heights[u] / 2.0 - heights[v] / 2.0);
                }
                if (i > 0) {  // 兜底：至少有一个能接在前一个节点后面的位置
                    options.push_back(cand[i - 1].back() + heights[layer[i - 1]] + opts.v_gap);
                } else {
                    options.push_back(opts.margin);
                }
                std::sort(options.begin(), options.end());
                options.erase(std::unique(options.begin(), options.end()), options.end());
                cand[i] = std::move(options);
                gain[i].assign(cand[i].size(), 0);
                for (std::size_t j = 0; j < cand[i].size(); ++j) {
                    const double center = cand[i][j] + heights[v] / 2.0;
                    for (const std::size_t u : up_neighbors[v]) {
                        const double want = y_down[u] + heights[u] / 2.0;
                        if (std::fabs(center - want) < 0.5) {
                            ++gain[i][j];
                        }
                    }
                }
            }
            std::vector<std::vector<State>> dp(n);
            std::vector<std::vector<std::size_t>> from(n);
            for (std::size_t i = 0; i < n; ++i) {
                dp[i].assign(cand[i].size(), State{});
                from[i].assign(cand[i].size(), 0);
                for (std::size_t j = 0; j < cand[i].size(); ++j) {
                    if (i == 0) {
                        dp[i][j] = State{gain[i][j], cand[i][j]};
                        continue;
                    }
                    State best;
                    std::size_t best_p = 0;
                    for (std::size_t p = 0; p < cand[i - 1].size(); ++p) {
                        if (dp[i - 1][p].gained < 0) {
                            continue;
                        }
                        if (cand[i - 1][p] + heights[layer[i - 1]] + opts.v_gap >
                            cand[i][j] + 1.0e-9) {
                            continue;  // 与同层前一个节点重叠
                        }
                        const State next{dp[i - 1][p].gained + gain[i][j],
                                         dp[i - 1][p].total + cand[i][j]};
                        if (best.gained < 0 || next.gained > best.gained ||
                            (next.gained == best.gained && next.total < best.total)) {
                            best = next;
                            best_p = p;
                        }
                    }
                    dp[i][j] = best;
                    from[i][j] = best_p;
                }
            }
            std::size_t best_j = 0;
            for (std::size_t j = 1; j < cand[n - 1].size(); ++j) {
                const State& s = dp[n - 1][j];
                const State& b = dp[n - 1][best_j];
                if (s.gained >= 0 &&
                    (b.gained < 0 || s.gained > b.gained ||
                     (s.gained == b.gained && s.total < b.total))) {
                    best_j = j;
                }
            }
            for (std::size_t i = n, j = best_j; i-- > 0;) {
                y_down[layer[i]] = cand[i][j];
                if (i == 0) {
                    break;
                }
                j = from[i][j];
            }
        }
        for (std::size_t l = 0; l < layer_count; ++l) {
            for (const std::size_t v : ext_layers[l]) {
                final_y[v] = y_down[v];
            }
        }
        // 膨胀上限：对齐的收益是用"图更高"换来的，但换太多就不值——超过
        // align_slack 的层整体回退到紧堆位置（该层的对齐收益放弃，图高不受影响）。
        for (std::size_t l = 0; l < layer_count; ++l) {
            const std::vector<std::size_t>& layer = ext_layers[l];
            double stacked = opts.margin;
            for (const std::size_t v : layer) {
                stacked += heights[v] + opts.v_gap;
            }
            stacked -= opts.v_gap;  // 最后一段之后没有间距
            const double stacked_span = stacked - opts.margin;
            const double aligned_span =
                (y_down[layer.back()] + heights[layer.back()]) - y_down[layer.front()];
            if (aligned_span <= stacked_span * (1.0 + std::max(0.0, opts.align_slack)) + 1.0e-9) {
                continue;  // 膨胀在容忍范围内，保留对齐结果
            }
            double y = opts.margin;
            for (const std::size_t v : layer) {
                y_down[v] = y;
                final_y[v] = y;
                y += heights[v] + opts.v_gap;
            }
        }
    }  // opts.align_rows
    for (std::size_t l = 0; l < layer_count; ++l) {
        for (const std::size_t v : ext_layers[l]) {
            if (v < count) {
                graph.nodes()[v].set_box(Rect{column_x[l], final_y[v], sizes[v].w, sizes[v].h});
            } else {
                dummy_row[v - count] = final_y[v];
            }
        }
    }

    // 长边走线骨架：沿 dummy 的槽位行横穿每个中间列，竖直段落在列间通道列。
    // 短边不写 route（仍由 graph::route 的正交路由决定）；调用方用
    // RouteOptions::keep_existing_routes 保留这里的折线。
    const auto gap_column = [&](std::size_t left) {
        return std::round((column_x[left] + column_width[left] + column_x[left + 1]) / 2.0);
    };
    for (const LongEdge& le : long_edges) {
        const Rect from_box = graph.nodes()[le.from].box();
        const Rect to_box = graph.nodes()[le.to].box();
        const double start_x = le.source_left ? from_box.right() : from_box.x;
        const double end_x = le.source_left ? to_box.x : to_box.right();
        const double from_y = std::round(from_box.y + from_box.h / 2.0);
        const double to_y = std::round(to_box.y + to_box.h / 2.0);

        std::vector<Point> path;
        path.push_back(Point{start_x, from_y});
        // dummies 是按 rank 升序登记的，而这里要按 from → to 的走向依次经过它们：
        // 逆行长边（from 在右列，例如被反转过的回边）必须倒着走，否则褶线会先跳到
        // 最左边的中间列再折回来。
        std::vector<std::size_t> chain = le.dummies;
        if (!le.source_left) {
            std::reverse(chain.begin(), chain.end());
        }
        for (const std::size_t d : chain) {
            const std::size_t col = dummy_column[d - count];
            const double row = std::round(dummy_row[d - count]);
            // 横走到该列一侧的通道列 → 竖直到槽位行 → 横穿该列（走槽位行）。
            const double before = gap_column(le.source_left ? col - 1 : col);
            path.push_back(Point{before, path.back().y});
            path.push_back(Point{before, row});
            const double after = gap_column(le.source_left ? col : col - 1);
            path.push_back(Point{after, row});
        }
        path.push_back(Point{path.back().x, to_y});
        path.push_back(Point{end_x, to_y});

        std::vector<Point> slim;  // 去掉零长段：相邻点必须有一个坐标不同
        for (const Point& p : path) {
            if (slim.empty() || std::lround(slim.back().x) != std::lround(p.x) ||
                std::lround(slim.back().y) != std::lround(p.y)) {
                slim.push_back(p);
            }
        }
        graph.edges()[le.edge].set_route(std::move(slim));
    }
    // 约定：layout_sugiyama 之后 route 非空 ⟺ 长边骨架。非长边显式清空，
    // 免得调用方拿着上一次路由留下的折线当骨架。
    for (std::size_t ei = 0; ei < graph.edges().size(); ++ei) {
        const Edge& e = graph.edges()[ei];
        const auto fi = index.find(e.from());
        const auto ti = index.find(e.to());
        const bool is_long = fi != index.end() && ti != index.end() &&
                             std::abs(rank[fi->second] - rank[ti->second]) > 1;
        if (!is_long) {
            graph.edges()[ei].set_route({});
        }
    }

    // —— 连通分量预分离（后处理重排）——
    // 多个互不相连的分量若被 NS 排进同一批列、同层节点互相穿插，视觉会糊在一起。
    // 这里按分量把节点整体在 X 方向错开 cluster_gap 重排（各分量内部几何完全不变，
    // 故单分量图输出与改动前逐字节一致 → 既有 golden/回归不受影响）。长边骨架路由
    // 属于分量内边，随分量一起平移，保持与节点对齐。
    {
        const std::vector<std::vector<std::size_t>> comps =
            connected_components_local(count, index, graph);
        if (comps.size() > 1) {
            std::vector<int> node_comp(count, -1);
            for (std::size_t c = 0; c < comps.size(); ++c) {
                for (const std::size_t v : comps[c]) {
                    node_comp[v] = static_cast<int>(c);
                }
            }
            // 分量按"最左节点原始 X"升序（保持左右阅读顺序），同值按最小节点下标。
            std::vector<std::size_t> order(comps.size());
            for (std::size_t i = 0; i < comps.size(); ++i) {
                order[i] = i;
            }
            std::vector<double> comp_min_x(comps.size(), 0.0);
            for (std::size_t c = 0; c < comps.size(); ++c) {
                double m = graph.nodes()[comps[c].front()].box().x;
                for (const std::size_t v : comps[c]) {
                    m = std::min(m, graph.nodes()[v].box().x);
                }
                comp_min_x[c] = m;
            }
            std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
                if (comp_min_x[a] != comp_min_x[b]) {
                    return comp_min_x[a] < comp_min_x[b];
                }
                return comps[a].front() < comps[b].front();
            });
            const double cluster_gap = std::max(opts.h_gap * 2.0, 4.0);
            double cursor = 0.0;
            std::vector<double> offset(comps.size(), 0.0);
            for (const std::size_t c : order) {
                double w = 0.0;
                for (const std::size_t v : comps[c]) {
                    const Rect b = graph.nodes()[v].box();
                    w = std::max(w, b.right() - comp_min_x[c]);
                }
                offset[c] = cursor - comp_min_x[c];
                cursor += w + cluster_gap;
            }
            // 平移节点框
            for (std::size_t c = 0; c < comps.size(); ++c) {
                const double dx = offset[c];
                if (dx == 0.0) {
                    continue;
                }
                for (const std::size_t v : comps[c]) {
                    const Rect b = graph.nodes()[v].box();
                    graph.nodes()[v].set_box(Rect{b.x + dx, b.y, b.w, b.h});
                }
            }
            // 平移分量内的长边骨架路由（端点必同分量）
            for (std::size_t ei = 0; ei < graph.edges().size(); ++ei) {
                const Edge& e = graph.edges()[ei];
                if (e.route().empty()) {
                    continue;
                }
                const auto fi = index.find(e.from());
                const auto ti = index.find(e.to());
                if (fi == index.end() || ti == index.end()) {
                    continue;
                }
                const int c = node_comp[fi->second];
                if (c < 0 || node_comp[ti->second] != c) {
                    continue;
                }
                const double dx = offset[static_cast<std::size_t>(c)];
                if (dx == 0.0) {
                    continue;
                }
                std::vector<Point> r = e.route();
                for (Point& p : r) {
                    p.x += dx;
                }
                graph.edges()[ei].set_route(std::move(r));
            }
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
