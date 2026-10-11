// bench_force.cpp —— 力导向布局的收敛基线
//
// 度量 `graph::layout_force`：耗时主要由"迭代轮数 × 每轮 O(V²) 斥力"决定，因此这里
// 固定迭代轮数、只扫节点规模，用来暴露平方项的增长（节点翻倍 → 每轮耗时约四倍）。
#include <cstddef>
#include <string>
#include <vector>

#include "bench.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/force.hpp"

namespace {

using namespace termchart;

// 环 + 每个节点连到对侧近邻：既连通又有交叉，避免布局过快塌缩成一点。
graph::Graph make_ring_plus_chords(std::size_t count) {
    graph::Graph graph;
    std::vector<graph::NodeId> ids;
    ids.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        ids.push_back(graph.add_node("n").id());
    }
    for (std::size_t i = 0; i < count; ++i) {
        (void)graph.add_edge(ids[i], ids[(i + 1) % count]);
        (void)graph.add_edge(ids[i], ids[(i + count / 2) % count]);
    }
    return graph;
}

}  // namespace

int main(int argc, char** argv) {
    const bool quick = bench::is_quick(argc, argv);
    const std::vector<std::size_t> sizes = quick ? std::vector<std::size_t>{12}
                                                 : std::vector<std::size_t>{20, 40, 80, 160};
    const int rounds = quick ? 3 : 9;

    bench::header("bench_force", "graph::layout_force（力导向，固定迭代轮数）");
    for (const std::size_t count : sizes) {
        const graph::Graph base = make_ring_plus_chords(count);
        graph::ForceOptions opts;
        opts.iterations = quick ? 40 : 200;
        const std::string size = std::to_string(base.node_count()) + " nodes / " +
                                 std::to_string(base.edge_count()) + " edges, " +
                                 std::to_string(opts.iterations) + " iters";
        const double ns = bench::median_ns(
            [&base, &opts] {
                graph::Graph copy = base;  // 每轮都要从干净副本开始
                (void)graph::layout_force(copy, opts);
            },
            rounds);
        bench::report("layout_force", size, ns);
    }
    return 0;
}
