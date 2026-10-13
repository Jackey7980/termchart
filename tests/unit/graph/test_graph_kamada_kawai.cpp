// test_graph_kamada_kawai.cpp —— KK 全局能量布局
#include <doctest.h>

#include <cmath>
#include <cstddef>

#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/kamada_kawai.hpp"

namespace {

using namespace termchart;
using namespace termchart::graph;

double center_x(const Node& node) { return node.box().x + node.box().w / 2.0; }
double center_y(const Node& node) { return node.box().y + node.box().h / 2.0; }

double center_distance(const Node& a, const Node& b) {
    const double dx = center_x(a) - center_x(b);
    const double dy = center_y(a) - center_y(b);
    return std::sqrt(dx * dx + dy * dy);
}

// 所有节点外框两两不重叠（边框相接不算重叠）。
bool boxes_disjoint(const Graph& graph) {
    for (std::size_t i = 0; i < graph.nodes().size(); ++i) {
        for (std::size_t j = i + 1; j < graph.nodes().size(); ++j) {
            if (graph.nodes()[i].box().intersects(graph.nodes()[j].box())) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

TEST_CASE("graph/layout_kamada_kawai: 空图返回空矩形") {
    Graph graph;
    const Rect bounds = layout_kamada_kawai(graph, KamadaKawaiOptions{});
    CHECK(bounds.w == 0.0);
    CHECK(bounds.h == 0.0);
}

TEST_CASE("graph/layout_kamada_kawai: 单节点落在边距处") {
    Graph graph;
    (void)graph.add_node("solo");
    const Rect bounds = layout_kamada_kawai(graph, KamadaKawaiOptions{});
    REQUIRE(graph.node_count() == 1);
    const Rect& box = graph.nodes()[0].box();
    CHECK(box.x == doctest::Approx(1.0));  // margin
    CHECK(box.y == doctest::Approx(1.0));
    CHECK(box.w >= 10.0);
    CHECK(box.h >= 4.0);
    CHECK(bounds == box);
}

TEST_CASE("graph/layout_kamada_kawai: 两节点按目标距离对放") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    (void)graph.add_edge(a, b);

    KamadaKawaiOptions opts;
    opts.edge_length = 12.0;
    (void)layout_kamada_kawai(graph, opts);

    CHECK(boxes_disjoint(graph));
    const double d = center_distance(graph.nodes()[0], graph.nodes()[1]);
    CHECK(d == doctest::Approx(12.0).epsilon(0.05));
}

TEST_CASE("graph/layout_kamada_kawai: 路径图相邻距离小于跨接距离") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    const NodeId c = graph.add_node("c").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, c);

    (void)layout_kamada_kawai(graph, KamadaKawaiOptions{});

    CHECK(boxes_disjoint(graph));
    const Node& na = *graph.find(a);
    const Node& nb = *graph.find(b);
    const Node& nc = *graph.find(c);
    // 跳数 1 与 2：目标距离分别是 12 与 24，实际距离应保持这个序与量级。
    CHECK(center_distance(na, nb) < center_distance(na, nc));
    CHECK(center_distance(nb, nc) < center_distance(na, nc));
    CHECK(center_distance(na, nb) == doctest::Approx(12.0).epsilon(0.3));
    CHECK(center_distance(na, nc) == doctest::Approx(24.0).epsilon(0.3));
}

TEST_CASE("graph/layout_kamada_kawai: 全连接三节点收敛到近似等边") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    const NodeId c = graph.add_node("c").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, c);
    (void)graph.add_edge(a, c);

    (void)layout_kamada_kawai(graph, KamadaKawaiOptions{});

    CHECK(boxes_disjoint(graph));
    const double ab = center_distance(*graph.find(a), *graph.find(b));
    const double bc = center_distance(*graph.find(b), *graph.find(c));
    const double ac = center_distance(*graph.find(a), *graph.find(c));
    const double lo = std::min(ab, std::min(bc, ac));
    const double hi = std::max(ab, std::max(bc, ac));
    CHECK(hi < lo * 1.5);
    CHECK(lo == doctest::Approx(12.0).epsilon(0.3));
}

TEST_CASE("graph/layout_kamada_kawai: 非连通图分量并排且互不重叠") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    const NodeId c = graph.add_node("c").id();
    const NodeId d = graph.add_node("d").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(c, d);

    (void)layout_kamada_kawai(graph, KamadaKawaiOptions{});

    CHECK(boxes_disjoint(graph));
    // 分量按首个节点的登记顺序从左到右摆放：前两个节点整体在右侧两个的左边。
    double first_max_right = 0.0;
    double second_min_x = 1e9;
    for (const NodeId id : {a, b}) {
        first_max_right = std::max(first_max_right, graph.find(id)->box().right());
    }
    for (const NodeId id : {c, d}) {
        second_min_x = std::min(second_min_x, graph.find(id)->box().x);
    }
    CHECK(first_max_right <= second_min_x);
}

TEST_CASE("graph/layout_kamada_kawai: 同一张图两次布局结果一致") {
    Graph first;
    Graph second;
    for (Graph* graph : {&first, &second}) {
        const NodeId a = graph->add_node("a").id();
        const NodeId b = graph->add_node("b").id();
        const NodeId c = graph->add_node("c").id();
        const NodeId d = graph->add_node("d").id();
        (void)graph->add_edge(a, b);
        (void)graph->add_edge(b, c);
        (void)graph->add_edge(c, d);
        (void)graph->add_edge(d, a);
    }
    (void)layout_kamada_kawai(first, KamadaKawaiOptions{});
    (void)layout_kamada_kawai(second, KamadaKawaiOptions{});

    REQUIRE(first.node_count() == second.node_count());
    for (std::size_t i = 0; i < first.node_count(); ++i) {
        CHECK(first.nodes()[i].box() == second.nodes()[i].box());
    }
    CHECK(boxes_disjoint(first));
}

TEST_CASE("graph/layout_kamada_kawai: 迭代耗尽仍产出不重叠的框") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    const NodeId c = graph.add_node("c").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, c);

    KamadaKawaiOptions opts;
    opts.max_iterations = 0;
    (void)layout_kamada_kawai(graph, opts);

    CHECK(boxes_disjoint(graph));
}
