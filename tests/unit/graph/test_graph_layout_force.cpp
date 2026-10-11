// test_graph_layout_force.cpp —— 力导向布局
#include <doctest.h>

#include <cmath>
#include <cstddef>

#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/force.hpp"

namespace {

using namespace termchart;
using namespace termchart::graph;

// 三角形 + 一条尾巴，够小但包含"环"与"悬挂点"两种结构。
void build_triangle_tail(Graph& graph) {
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    const NodeId c = graph.add_node("c").id();
    const NodeId d = graph.add_node("d").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, c);
    (void)graph.add_edge(c, a);
    (void)graph.add_edge(c, d);
}

bool overlaps(const Rect& a, const Rect& b) noexcept {
    return a.x < b.right() && b.x < a.right() && a.y < b.bottom() && b.y < a.bottom();
}

}  // namespace

TEST_CASE("graph/layout/force: 空图返回空矩形") {
    Graph graph;
    CHECK(layout_force(graph).w == doctest::Approx(0.0));
}

TEST_CASE("graph/layout/force: 同图两次布局完全一致（确定性）") {
    Graph first;
    build_triangle_tail(first);
    (void)layout_force(first);

    Graph second;
    build_triangle_tail(second);
    (void)layout_force(second);

    REQUIRE(first.node_count() == second.node_count());
    for (std::size_t i = 0; i < first.node_count(); ++i) {
        CHECK(first.nodes()[i].box().x == doctest::Approx(second.nodes()[i].box().x));
        CHECK(first.nodes()[i].box().y == doctest::Approx(second.nodes()[i].box().y));
    }
}

TEST_CASE("graph/layout/force: 落格为整数、在边距内、并集覆盖全部节点") {
    Graph graph;
    build_triangle_tail(graph);
    const ForceOptions opts;
    const Rect bounds = layout_force(graph, opts);

    for (const Node& node : graph.nodes()) {
        const Rect box = node.box();
        // 落格：坐标必须是整数（字符网格上没有半格）。
        CHECK(box.x == std::round(box.x));
        CHECK(box.y == std::round(box.y));
        CHECK(box.x >= doctest::Approx(opts.layout.margin));
        CHECK(box.y >= doctest::Approx(opts.layout.margin));
        CHECK(box.x + box.w <= doctest::Approx(bounds.right()));
        CHECK(box.y + box.h <= doctest::Approx(bounds.bottom()));
    }
    CHECK(bounds.w > 0.0);
    CHECK(bounds.h > 0.0);
}

TEST_CASE("graph/layout/force: 三角形三点互不重叠") {
    Graph graph;
    build_triangle_tail(graph);
    (void)layout_force(graph);

    for (std::size_t i = 0; i < graph.node_count(); ++i) {
        for (std::size_t j = i + 1; j < graph.node_count(); ++j) {
            CHECK_FALSE(overlaps(graph.nodes()[i].box(), graph.nodes()[j].box()));
        }
    }
}

TEST_CASE("graph/layout/force: 迭代数为 0 时退化为初始圆环且不崩") {
    Graph graph;
    build_triangle_tail(graph);
    ForceOptions opts;
    opts.iterations = 0;
    const Rect bounds = layout_force(graph, opts);
    CHECK(bounds.w > 0.0);
    for (const Node& node : graph.nodes()) {
        CHECK(node.box().x >= doctest::Approx(opts.layout.margin));
        CHECK(node.box().y >= doctest::Approx(opts.layout.margin));
    }
}
