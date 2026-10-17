// cycle_removal.hpp —— Sugiyama 第一步：破环（回边反转）
//
// 为什么抽成共享私有头：`layout_layered` 与 `layout_sugiyama` 的最长路径分层都要求
// 输入是无环图，此前各自带一份"Kahn + 破环兜底"——没源点时挑入度最小者当种子，
// 处理不到的节点再挂到 max(已处理前驱)+1。那套兜底在混合图上会塌：环上的整条链
// 前驱都未处理 ⇒ best < 0 ⇒ 全部落到第 0 层（实测 60 示例 --cycle 时 L0 塞进 17 个
// 节点，其中 7 个来自 L2–L6）。打分两步走（先打成 DAG，再分层）就不需要兜底了。
//
// 算法：迭代式 DFS（递归在深链上会爆栈），节点三色白/灰/黑：
//   * 指向灰色（尚在当前 DFS 栈上）节点的边 = 回边 ⇒ 反转；
//   * 自环丢弃（它不参与分层：留着会让 rank 永远抬不起来）；
//   * 其余边保持原方向。
// 得到的是一个**可行**反馈弧集。最小反馈弧集是 NP-hard，DFS 不追求最小；要更少
// 的反转可以后续加"优先访问出度大的邻居"这类启发式，接口不变。
//
// 确定性：DFS 起点按节点下标升序、邻接按边登记顺序，因此同一张图每次得到同一份
// DAG（golden 与回归测试依赖这一点）。
//
// 本头 private 给 src/graph/layout/ 用，不进公开 include：怎样去环是实现细节，
// 用户只需要"布局结果对"，不需要知道哪条边被反转过。
#ifndef TERMCHART_SRC_GRAPH_LAYOUT_CYCLE_REMOVAL_HPP
#define TERMCHART_SRC_GRAPH_LAYOUT_CYCLE_REMOVAL_HPP

#include <cstddef>
#include <utility>
#include <vector>

namespace termchart::graph {
namespace layout_internal {

// 打成 DAG 之后的一条有向边（下标是 graph.nodes() 里的下标）。
struct DirectedEdge {
    std::size_t from = 0;
    std::size_t to = 0;
    bool reversed = false;  // 相对用户登记的方向被反转过
};

struct CycleRemoval {
    std::vector<DirectedEdge> edges;   // 无环：按任意顺序拓扑求解都成立
    std::size_t reversed_count = 0;    // 反转了几条（越多说明代理目标离真目标越远）
    std::size_t self_loops = 0;        // 丢弃的自环数
};

inline CycleRemoval remove_cycles(std::size_t node_count,
                                  const std::vector<std::pair<std::size_t, std::size_t>>& edges) {
    std::vector<std::vector<std::size_t>> adjacency(node_count);
    CycleRemoval out;
    out.edges.reserve(edges.size());
    for (const auto& edge : edges) {
        if (edge.first == edge.second) {
            ++out.self_loops;  // 自环：既抬不动 rank，也进不了任何相邻层口径
            continue;
        }
        adjacency[edge.first].push_back(edge.second);
    }

    // 0 = 未访问，1 = 在当前 DFS 栈上，2 = 已出栈（子树完成）。
    std::vector<unsigned char> color(node_count, 0);
    for (std::size_t root = 0; root < node_count; ++root) {
        if (color[root] != 0) {
            continue;
        }
        std::vector<std::pair<std::size_t, std::size_t>> stack;  // (节点, 下一条待看邻接)
        color[root] = 1;
        stack.emplace_back(root, 0);
        while (!stack.empty()) {
            const std::size_t u = stack.back().first;
            std::size_t& cursor = stack.back().second;
            if (cursor >= adjacency[u].size()) {
                color[u] = 2;
                stack.pop_back();
                continue;
            }
            const std::size_t v = adjacency[u][cursor];
            ++cursor;  // 先推进：下面可能 push_back，那会让 cursor 引用失效
            if (color[v] == 1) {
                // 回边：反转它就把这一处环打开成链。
                out.edges.push_back(DirectedEdge{v, u, true});
                ++out.reversed_count;
                continue;
            }
            out.edges.push_back(DirectedEdge{u, v, false});
            if (color[v] == 0) {
                color[v] = 1;
                stack.emplace_back(v, 0);  // 此后不再触碰 cursor / stack.back()
            }
        }
    }
    return out;
}

// 最长路径分层（Kahn 拓扑序，rank = 距任意源的最长距离）。
// 输入必须无环（先过 remove_cycles）。既是 layout_layered 的分层器，也是
// network simplex 的初始可行 rank。
inline std::vector<int> longest_path_ranks(std::size_t count,
                                           const std::vector<DirectedEdge>& dag_edges) {
    std::vector<std::vector<std::size_t>> out_edges(count);
    std::vector<int> in_deg(count, 0);
    for (const DirectedEdge& e : dag_edges) {
        out_edges[e.from].push_back(e.to);
        ++in_deg[e.to];
    }
    std::vector<int> rank(count, 0);
    std::vector<int> remaining = in_deg;
    std::vector<std::size_t> queue(count);
    std::size_t head = 0, tail = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (remaining[i] == 0) {
            queue[tail++] = i;
        }
    }
    while (head < tail) {
        const std::size_t u = queue[head++];
        for (const std::size_t v : out_edges[u]) {
            if (rank[u] + 1 > rank[v]) {
                rank[v] = rank[u] + 1;
            }
            if (--remaining[v] == 0) {
                queue[tail++] = v;
            }
        }
    }
    return rank;
}

}  // namespace layout_internal
}  // namespace termchart::graph

#endif  // TERMCHART_SRC_GRAPH_LAYOUT_CYCLE_REMOVAL_HPP
