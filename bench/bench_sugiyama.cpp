// bench_sugiyama.cpp —— 分层布局的扩展性基线
//
// 度量 `graph::layout_layered`（本仓对应规格里的 sugiyama 分层部分）在不同规模的
// 分层 DAG 上的耗时。关注点：节点数翻倍时耗时增长是否接近线性（Kahn + 列内累加
// 都是 O(V+E)，不该出现平方级行为）。
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "bench.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"

namespace {

using namespace termchart;

// 分层 DAG：layers 层、每层 width 个节点；每个节点连下一层的自身与右邻。
graph::Graph make_layered_dag(std::size_t layers, std::size_t width) {
    graph::Graph graph;
    std::vector<std::vector<graph::NodeId>> rows;
    rows.reserve(layers);
    for (std::size_t l = 0; l < layers; ++l) {
        std::vector<graph::NodeId> row;
        row.reserve(width);
        for (std::size_t w = 0; w < width; ++w) {
            row.push_back(graph.add_node("n").id());
        }
        rows.push_back(std::move(row));
    }
    for (std::size_t l = 0; l + 1 < layers; ++l) {
        for (std::size_t w = 0; w < width; ++w) {
            (void)graph.add_edge(rows[l][w], rows[l + 1][w]);
            if (w + 1 < width) {
                (void)graph.add_edge(rows[l][w], rows[l + 1][w + 1]);
            }
        }
    }
    return graph;
}

}  // namespace

int main(int argc, char** argv) {
    const bool quick = bench::is_quick(argc, argv);
    const std::vector<std::pair<std::size_t, std::size_t>> shapes =
        quick ? std::vector<std::pair<std::size_t, std::size_t>>{{4, 4}}
              : std::vector<std::pair<std::size_t, std::size_t>>{
                    {8, 8}, {16, 16}, {24, 24}, {32, 32}};
    const int rounds = quick ? 3 : 15;

    bench::header("bench_sugiyama", "graph::layout_layered（分层布局，含破环）");
    for (const auto& shape : shapes) {
        const graph::Graph base = make_layered_dag(shape.first, shape.second);
        const std::string size = std::to_string(base.node_count()) + " nodes / " +
                                 std::to_string(base.edge_count()) + " edges";
        const double ns = bench::median_ns(
            [&base] {
                graph::Graph copy = base;  // 布局会就地写 Node::set_box，必须用干净副本
                (void)graph::layout_layered(copy);
            },
            rounds);
        bench::report("layout_layered", size, ns);
    }
    return 0;
}
