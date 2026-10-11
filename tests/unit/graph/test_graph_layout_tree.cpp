// test_graph_layout_tree.cpp —— 根树布局（单向与左右分列）
#include <doctest.h>

#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/tree_layout.hpp"

namespace {

using namespace termchart;
using namespace termchart::graph;

// 默认尺寸：节点 10x4，列间距 4（步长 14），行间距 2（步长 6），边距 1。
constexpr double kMargin = 1.0;
constexpr double kColStep = 14.0;
constexpr double kRowStep = 6.0;

}  // namespace

TEST_CASE("graph/layout/tree: 深度决定列，叶子各占一行，父节点取子树中心") {
    Graph graph;
    const NodeId root = graph.add_node("root").id();
    const NodeId a = graph.add_node("a").id();
    const NodeId a1 = graph.add_node("a1").id();
    const NodeId b = graph.add_node("b").id();
    (void)graph.add_edge(root, a);
    (void)graph.add_edge(a, a1);
    (void)graph.add_edge(root, b);

    (void)layout_tree(graph, TreeOptions{});

    // 根在第 0 列，子节点第 1 列，孙节点第 2 列
    CHECK(graph.find(root)->box().x == doctest::Approx(kMargin));
    CHECK(graph.find(a)->box().x == doctest::Approx(kMargin + kColStep));
    CHECK(graph.find(a1)->box().x == doctest::Approx(kMargin + 2 * kColStep));
    CHECK(graph.find(b)->box().x == doctest::Approx(kMargin + kColStep));

    // 叶子 a1 与 b 各占一行；a 与 a1 同行；根取 a 与 b 的中点
    CHECK(graph.find(a1)->box().y == doctest::Approx(kMargin));
    CHECK(graph.find(a)->box().y == doctest::Approx(kMargin));
    CHECK(graph.find(b)->box().y == doctest::Approx(kMargin + kRowStep));
    CHECK(graph.find(root)->box().y == doctest::Approx(kMargin + kRowStep / 2.0));
}

TEST_CASE("graph/layout/tree: left_right 时根居中、偶数子树向右、奇数向左") {
    Graph graph;
    const NodeId root = graph.add_node("root").id();
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    const NodeId c = graph.add_node("c").id();
    const NodeId d = graph.add_node("d").id();
    (void)graph.add_edge(root, a);
    (void)graph.add_edge(root, b);
    (void)graph.add_edge(root, c);
    (void)graph.add_edge(root, d);

    TreeOptions opts;
    opts.left_right = true;
    (void)layout_tree(graph, opts);

    const Rect root_box = graph.find(root)->box();
    CHECK(graph.find(a)->box().x > root_box.x);  // 偶数 → 向右
    CHECK(graph.find(c)->box().x > root_box.x);
    CHECK(graph.find(b)->box().x < root_box.x);  // 奇数 → 向左
    CHECK(graph.find(d)->box().x < root_box.x);

    // 左侧节点仍在边距内（根列给左侧留足了宽度）
    CHECK(graph.find(b)->box().x >= doctest::Approx(kMargin));
    CHECK(graph.find(d)->box().x >= doctest::Approx(kMargin));
    // 同侧同一列
    CHECK(graph.find(a)->box().x == doctest::Approx(graph.find(c)->box().x));
    CHECK(graph.find(b)->box().x == doctest::Approx(graph.find(d)->box().x));
}

TEST_CASE("graph/layout/tree: 有环图（无入度 0 节点）不崩且节点不重叠") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, a);

    const Rect bounds = layout_tree(graph, TreeOptions{});
    CHECK(bounds.w > 0.0);
    const Rect ab = graph.find(a)->box();
    const Rect bb = graph.find(b)->box();
    const bool same_origin = (ab.x == bb.x) && (ab.y == bb.y);
    CHECK_FALSE(same_origin);  // 环上两节点不能重叠在一格
}

TEST_CASE("graph/layout/tree: 空图返回空矩形") {
    Graph graph;
    CHECK(layout_tree(graph, TreeOptions{}).w == doctest::Approx(0.0));
}
