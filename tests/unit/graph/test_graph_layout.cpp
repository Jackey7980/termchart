// test_graph_layout.cpp —— 网格布局与分层布局
#include <doctest.h>

#include <cstddef>

#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"

namespace {

using namespace termchart;
using namespace termchart::graph;

bool overlaps(const Rect& a, const Rect& b) noexcept {
    return a.x < b.right() && b.x < a.right() && a.y < b.bottom() && b.y < a.bottom();
}

}  // namespace

TEST_CASE("graph/layout: node_box_for 按标签显示宽度取尺寸") {
    Graph graph;
    const NodeId id = graph.add_node("abcd").id();
    const LayoutOptions opts;
    const Rect size = node_box_for(*graph.find(id), opts);
    CHECK(size.w == doctest::Approx(10.0));  // max(最小宽 10, 标签 4 + 2)
    CHECK(size.h == doctest::Approx(4.0));

    const NodeId wide = graph.add_node("0123456789abc").id();  // 13 字符
    CHECK(node_box_for(*graph.find(wide), opts).w == doctest::Approx(15.0));
}

TEST_CASE("graph/layout: node_box_for 多行标签把框撑高") {
    Graph graph;
    const NodeId single = graph.add_node("AB").id();
    const NodeId two = graph.add_node("AB\nCD").id();
    const NodeId three = graph.add_node("A\nBCDE\nC").id();
    LayoutOptions opts;  // min_width 10、min_height 4
    CHECK(node_box_for(*graph.find(single), opts).h == doctest::Approx(4.0));
    CHECK(node_box_for(*graph.find(two), opts).h == doctest::Approx(4.0));   // 2 + 2 = 4
    CHECK(node_box_for(*graph.find(three), opts).h == doctest::Approx(5.0));  // 3 + 2 = 5
    // 宽度取最长行："BCDE" 4 + 2 = 6，仍由 min_width 10 兜底
    CHECK(node_box_for(*graph.find(three), opts).w == doctest::Approx(10.0));

    LayoutOptions narrow;
    narrow.min_width = 5.0;
    narrow.min_height = 3.0;
    CHECK(node_box_for(*graph.find(three), narrow).w == doctest::Approx(6.0));
    CHECK(node_box_for(*graph.find(three), narrow).h == doctest::Approx(5.0));
    CHECK(node_box_for(*graph.find(single), narrow).h == doctest::Approx(3.0));
}

TEST_CASE("graph/layout: 网格布局按列推进且互不重叠") {
    Graph graph;
    for (int i = 0; i < 5; ++i) {
        (void)graph.add_node("n");
    }
    const Rect bounds = layout_grid(graph, 2, LayoutOptions{});

    // 单元格 = 10 x 4，间距 4 / 2，边距 1
    CHECK(graph.nodes()[0].box().x == doctest::Approx(1.0));
    CHECK(graph.nodes()[1].box().x == doctest::Approx(15.0));  // 1 + 10 + 4
    CHECK(graph.nodes()[2].box().x == doctest::Approx(1.0));
    CHECK(graph.nodes()[0].box().y == doctest::Approx(1.0));
    CHECK(graph.nodes()[2].box().y == doctest::Approx(7.0));   // 1 + 4 + 2
    CHECK(graph.nodes()[4].box().y == doctest::Approx(13.0));
    CHECK(bounds.x == doctest::Approx(1.0));
    CHECK(bounds.y == doctest::Approx(1.0));

    for (std::size_t i = 0; i < graph.nodes().size(); ++i) {
        for (std::size_t j = i + 1; j < graph.nodes().size(); ++j) {
            CHECK_FALSE(overlaps(graph.nodes()[i].box(), graph.nodes()[j].box()));
        }
    }
}

TEST_CASE("graph/layout: 分层布局按最长路径推进列号") {
    Graph chain;
    const NodeId a = chain.add_node("a").id();
    const NodeId b = chain.add_node("b").id();
    const NodeId c = chain.add_node("c").id();
    (void)chain.add_edge(a, b);
    (void)chain.add_edge(b, c);
    (void)layout_layered(chain, LayoutOptions{});

    CHECK(chain.find(a)->box().x < chain.find(b)->box().x);
    CHECK(chain.find(b)->box().x < chain.find(c)->box().x);
    CHECK(chain.find(a)->box().y == doctest::Approx(chain.find(b)->box().y));

    Graph fork;
    const NodeId root = fork.add_node("root").id();
    const NodeId left = fork.add_node("left").id();
    const NodeId right = fork.add_node("right").id();
    (void)fork.add_edge(root, left);
    (void)fork.add_edge(root, right);
    (void)layout_layered(fork, LayoutOptions{});

    CHECK(fork.find(left)->box().x == doctest::Approx(fork.find(right)->box().x));
    CHECK(fork.find(left)->box().y != doctest::Approx(fork.find(right)->box().y));
}

TEST_CASE("graph/layout: 整图成环时按入度最小者破环，不再堆成一列") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    const NodeId c = graph.add_node("c").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, c);
    (void)graph.add_edge(c, a);  // 每个节点都有入边：没有天然源点

    (void)layout_layered(graph, LayoutOptions{});
    CHECK(graph.find(a)->box().x < graph.find(b)->box().x);
    CHECK(graph.find(b)->box().x < graph.find(c)->box().x);
}

TEST_CASE("graph/layout: 有环时环上节点退到第 0 层且不重叠") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, a);  // 成环
    (void)layout_layered(graph, LayoutOptions{});
    CHECK_FALSE(overlaps(graph.find(a)->box(), graph.find(b)->box()));
}
