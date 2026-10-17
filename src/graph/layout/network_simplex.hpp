// network_simplex.hpp —— Sugiyama 分层：network simplex 最小化边总长
//
// 对应论文：Gansner, Koutsofios, North, Vo, "A Technique for Drawing Directed
// Graphs"（1993）第 3 节。问题陈述：
//   min  Σ(rank[to] - rank[from])
//   s.t. rank[to] - rank[from] >= 1    （每条边至少跨一层）
// 最长路径只是这个问题的**一个可行解**（把所有节点尽量压到第 0 层），边总长往往
// 离最优很远：挂在链中段的叶子被压到 rank 0/1，入边被拉成跨多层的长边，继而带出
// 更多 dummy。network simplex 在可行域上迭代到最优，产出"边总长最短"的分层——
// 图更矮、dummy 更少、坐标分配的自由度更集中。
//
// 实现走"分量提升"形式的 pivot（与 graphviz mincut.c 同语义，但每轮全量重算割值
// 而不是维护增量，节点几百以内完全够用，少一类容易算错的缓存）：
//   1. init_rank = 最长路径（DAG 上可行）；
//   2. feasible_tree：沿 slack==0 的边扩张出可行生成树；未覆盖时把最小 slack 的
//      跨越边收窄到 tight 再长。**收窄方向是抬 from 所在那一侧**（详见下方注释，
//      抬错方向会让 slack 翻倍爆炸、树永远长不完——踩过）；
//   3. pivot：对每条树边 f，删 f 得到"含 f.to 侧"的分量 X，另加孤立单点分量；
//      若 cut = #(X→外) − #(外→X) > 0 则把 X 整体 rank += delta（delta = X→外
//      全部边的最小 slack），目标函数下降 cut × delta，delta 归零的非树边入树、
//      f 出树。只接受严格改进 ⇒ 整数目标严格下降 ⇒ 有限终止。
#ifndef TERMCHART_SRC_GRAPH_LAYOUT_NETWORK_SIMPLEX_HPP
#define TERMCHART_SRC_GRAPH_LAYOUT_NETWORK_SIMPLEX_HPP

#include <algorithm>
#include <cstddef>
#include <limits>
#include <queue>
#include <vector>

#include "cycle_removal.hpp"

namespace termchart::graph {
namespace layout_internal {

// 返回最小化边总长的分层（rank 最小值为 0）。输入必须无环（先 remove_cycles）。
inline std::vector<int> rank_assignment_ns(std::size_t count,
                                           const std::vector<DirectedEdge>& dag_edges) {
    if (count == 0) {
        return {};
    }
    const std::size_t m = dag_edges.size();
    std::vector<std::vector<std::size_t>> incident(count);  // 每个节点挂的边下标
    for (std::size_t i = 0; i < m; ++i) {
        incident[dag_edges[i].from].push_back(i);
        incident[dag_edges[i].to].push_back(i);
    }

    std::vector<int> rank = longest_path_ranks(count, dag_edges);
    const auto slack = [&](std::size_t ei) {
        return rank[dag_edges[ei].to] - rank[dag_edges[ei].from] - 1;
    };

    // 生成树状态：parent[v] = 树中父（-1 根、-2 未入树），on_tree[e] = 是否在树上。
    std::vector<std::ptrdiff_t> parent(count, -2);
    std::vector<char> on_tree(m, 0);

    // 沿 tight 边扩张树。两个约束同时要满足：
    //   1) parent 链必须完整：新节点只能由**树内**节点认领（BFS 穿过外部节点会
    //      让 parent 指向树外，parent 链断裂，后续再也走不到那批"树内"节点）；
    //   2) 已入树节点的邻接要**每轮重扫**：上轮抬升后才变 tight 的新边挂在老节点
    //      身上，用"访问过就跳过"会漏掉它（树永远长不完，feasible_tree 死循环）。
    // 所以这里不用 BFS，改成反复全量扫描树内节点的邻接直到没有新增。
    const auto tight_grow = [&](std::size_t root) {
        if (parent[root] == -2) {
            parent[root] = -1;
        }
        bool grew = true;
        while (grew) {
            grew = false;
            for (std::size_t u = 0; u < count; ++u) {
                if (parent[u] == -2) {
                    continue;  // 只扫树内节点
                }
                for (const std::size_t ei : incident[u]) {
                    if (slack(ei) != 0) {
                        continue;
                    }
                    const DirectedEdge& e = dag_edges[ei];
                    const std::size_t v = e.from == u ? e.to : e.from;
                    if (parent[v] == -2) {
                        parent[v] = static_cast<std::ptrdiff_t>(u);
                        on_tree[ei] = 1;
                        grew = true;
                    }
                }
            }
        }
    };

    // feasible_tree：反复收窄最小 slack 的跨越边，直到树覆盖全图。
    for (std::size_t seed = 0; seed < count; ++seed) {
        if (parent[seed] != -2) {
            continue;  // 已被之前的分量覆盖
        }
        tight_grow(seed);
        while (true) {
            int best = std::numeric_limits<int>::max();
            bool out_to_tree = false;  // 最小 slack 的跨越边是否是"外→树"方向
            bool any = false;
            for (std::size_t ei = 0; ei < m; ++ei) {
                const bool in_from = parent[dag_edges[ei].from] != -2;
                const bool in_to = parent[dag_edges[ei].to] != -2;
                if (in_from == in_to) {
                    continue;
                }
                any = true;
                const int s = slack(ei);
                if (s < best) {
                    best = s;
                    out_to_tree = !in_from;  // from 在树外、to 在树内 ⇒ 外→树
                }
            }
            if (!any) {
                break;  // 本连通分量处理完；外层循环挑下一个分量的种子
            }
            // 收窄 slack = rank[to] - rank[from] - 1 的唯一安全办法是抬 **from
            // 所在那一侧**（rank[from] += slack）：
            //   树→外（from 在树内）⇒ 抬树内；
            //   外→树（from 在树外）⇒ 抬树外。
            // 两侧都是抬，只是抬哪边；best 是全体跨越边的最小 slack，所以被抬一侧
            // 的跨越边 slack 全部 >= 0，不会翻负。改成"动 to 侧"（抬树内/降树外）
            // 会把外→树边越弄越松——slack 翻倍爆炸、树永远长不完（死循环之源）。
            const bool lift_outside = out_to_tree;  // from 在树外 ⇒ 抬树外
            for (std::size_t w = 0; w < count; ++w) {
                const bool in_tree = parent[w] != -2;
                if (in_tree != lift_outside) {
                    rank[w] += best;
                }
            }
            tight_grow(seed);
            bool still_crossing = false;
            for (std::size_t ei = 0; ei < m; ++ei) {
                const bool in_from = parent[dag_edges[ei].from] != -2;
                const bool in_to = parent[dag_edges[ei].to] != -2;
                if (in_from != in_to) {
                    still_crossing = true;
                    break;
                }
            }
            if (!still_crossing) {
                break;
            }
        }
    }

    // pivot：把候选分量 X 整体上抬，直到没有任何割能改进目标。
    // 候选两类：删树边 f 后含 f.to 的分量（head 侧）、孤立单点分量（孤立节点没有
    // 树边，只能显式加进来，否则它的边永远没人去收紧）。f.from 侧（补集）不能抬——
    // f 自身是那条分量的 X→外 tight 树边，一抬 slack 就翻负（不可行）。
    // 每轮全量重算，n/m 几百的规模下开销无关紧要；换来的是无增量缓存可错。
    const auto component_of = [&](std::size_t seed, std::size_t blocked,
                                  std::vector<char>& in_x) {
        std::fill(in_x.begin(), in_x.end(), 0);
        std::queue<std::size_t> q;
        q.push(seed);
        in_x[seed] = 1;
        while (!q.empty()) {
            const std::size_t u = q.front();
            q.pop();
            for (const std::size_t ei : incident[u]) {
                if (ei == blocked || !on_tree[ei]) {
                    continue;
                }
                const DirectedEdge& e = dag_edges[ei];
                const std::size_t v = e.from == u ? e.to : e.from;
                if (!in_x[v]) {
                    in_x[v] = 1;
                    q.push(v);
                }
            }
        }
    };

    // 对候选 X 求割：cut = #(X→外) − #(外→X)。抬 X 会收窄**所有** X→外 边的
    // slack（含出树的那条 f：f 出树后约束并没有解除），所以 delta 取 X→外 全部边
    // 的最小 slack，归零的非树边就是入树边。
    const auto evaluate = [&](const std::vector<char>& in_x, int& delta_out,
                              std::size_t& enter_out) {
        long cut = 0;
        int delta = std::numeric_limits<int>::max();
        std::size_t enter = m;
        for (std::size_t ei = 0; ei < m; ++ei) {
            const DirectedEdge& e = dag_edges[ei];
            const bool from_in = in_x[e.from] != 0;
            const bool to_in = in_x[e.to] != 0;
            if (from_in && !to_in) {          // X→外：抬 X 会收窄它
                ++cut;
                const int s = slack(ei);
                if (s < delta) {
                    delta = s;
                    enter = on_tree[ei] ? m : ei;  // 树边不能当入树边
                } else if (s == delta && enter == m && !on_tree[ei]) {
                    enter = ei;
                }
            } else if (!from_in && to_in) {   // 外→X：抬 X 会放长它
                --cut;
            }
        }
        // 无效候选（cut<=0、delta<=0、没有可入树的出边）一律给 0：调用方算
        // gain = cut × delta，给大数会让无效候选被选中。
        delta_out = (cut > 0 && enter != m) ? delta : 0;
        enter_out = enter;
        return cut;
    };

    std::vector<char> in_x(count);
    std::vector<char> best_x(count, 0);
    while (true) {
        double best_gain = 0.0;
        int best_delta = 0;
        std::size_t best_e = m, best_f = m;  // m = 无效哨兵（孤立分量无出树边）

        for (std::size_t f = 0; f < m; ++f) {
            if (!on_tree[f]) {
                continue;
            }
            component_of(dag_edges[f].to, f, in_x);  // 含 f.to 侧
            int delta = 0;
            std::size_t enter = m;
            const long cut = evaluate(in_x, delta, enter);
            const double gain = static_cast<double>(cut) * static_cast<double>(delta);
            if (gain > best_gain) {
                best_gain = gain;
                best_delta = delta;
                best_e = enter;
                best_f = f;
                best_x = in_x;
            }
        }
        for (std::size_t w = 0; w < count; ++w) {  // 孤立单点分量
            bool isolated = true;
            for (const std::size_t ei : incident[w]) {
                if (on_tree[ei]) {
                    isolated = false;
                    break;
                }
            }
            if (!isolated) {
                continue;
            }
            std::fill(in_x.begin(), in_x.end(), 0);
            in_x[w] = 1;
            int delta = 0;
            std::size_t enter = m;
            const long cut = evaluate(in_x, delta, enter);
            const double gain = static_cast<double>(cut) * static_cast<double>(delta);
            if (gain > best_gain) {
                best_gain = gain;
                best_delta = delta;
                best_e = enter;
                best_f = m;  // 无出树边
                best_x = in_x;
            }
        }

        if (best_gain <= 0.0) {
            break;  // 没有任何割能改进：到最优
        }
        for (std::size_t w = 0; w < count; ++w) {
            if (best_x[w]) {
                rank[w] += best_delta;
            }
        }
        if (best_f != m) {
            on_tree[best_f] = 0;
        }
        on_tree[best_e] = 1;
        // 重建 parent（从各根沿树边 BFS）
        std::fill(parent.begin(), parent.end(), -2);
        for (std::size_t seed = 0; seed < count; ++seed) {
            if (parent[seed] != -2) {
                continue;
            }
            std::queue<std::size_t> q;
            q.push(seed);
            parent[seed] = -1;
            while (!q.empty()) {
                const std::size_t u = q.front();
                q.pop();
                for (const std::size_t ei : incident[u]) {
                    if (!on_tree[ei]) {
                        continue;
                    }
                    const DirectedEdge& e = dag_edges[ei];
                    const std::size_t v = e.from == u ? e.to : e.from;
                    if (parent[v] == -2) {
                        parent[v] = static_cast<std::ptrdiff_t>(u);
                        q.push(v);
                    }
                }
            }
        }
    }

    // normalize：平移到 min rank = 0（布局不需要绝对坐标）。
    const int lo = *std::min_element(rank.begin(), rank.end());
    if (lo != 0) {
        for (int& r : rank) {
            r -= lo;
        }
    }
    return rank;
}

}  // namespace layout_internal
}  // namespace termchart::graph

#endif  // TERMCHART_SRC_GRAPH_LAYOUT_NETWORK_SIMPLEX_HPP
