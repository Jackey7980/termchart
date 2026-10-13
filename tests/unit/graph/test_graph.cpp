// test_graph.cpp —— 图容器、节点/边、元数据、端口
#include <doctest.h>

#include <string>

#include "termchart/graph/attributes.hpp"
#include "termchart/graph/edge.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/node.hpp"
#include "termchart/graph/port.hpp"

namespace {

using namespace termchart;
using namespace termchart::graph;

}  // namespace

TEST_CASE("graph/attributes: 读写、缺失回退、删除") {
    Attributes attributes;
    CHECK(attributes.empty());

    attributes.set("shape", "box");
    attributes.set("color", "red");
    CHECK(attributes.size() == 2);
    CHECK(attributes.has("shape"));
    CHECK(attributes.get("shape") == "box");
    CHECK(attributes.get("missing").empty());
    CHECK(attributes.get("missing", "fallback") == "fallback");

    CHECK(attributes.erase("shape"));
    CHECK_FALSE(attributes.has("shape"));
    CHECK_FALSE(attributes.erase("shape"));

    attributes.clear();
    CHECK(attributes.empty());
}

TEST_CASE("graph: 节点 id 自增、查找、标签与元数据") {
    Graph graph;
    CHECK(graph.node_count() == 0);

    const NodeId a = graph.add_node("start").id();
    const NodeId b = graph.add_node("end").id();
    CHECK(a == 1);  // 0 保留为无效
    CHECK(b == 2);

    REQUIRE(graph.find(a) != nullptr);
    CHECK(graph.find(a)->label() == "start");
    graph.find(a)->attributes().set("shape", "ellipse");
    CHECK(graph.find(a)->attributes().get("shape") == "ellipse");
    CHECK(graph.find(99) == nullptr);
}

TEST_CASE("graph: 边要求端点存在，度数正确") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();

    CHECK(graph.add_edge(a, 99) == nullptr);  // 端点缺失
    CHECK(graph.edge_count() == 0);

    Edge* e = graph.add_edge(a, b, "flow");
    REQUIRE(e != nullptr);
    CHECK(e->from() == a);
    CHECK(e->to() == b);
    CHECK(e->label() == "flow");
    CHECK(e->arrow() == ArrowStyle::AtTarget);

    CHECK(graph.out_degree(a) == 1);
    CHECK(graph.in_degree(a) == 0);
    CHECK(graph.in_degree(b) == 1);
    CHECK(graph.edges_of(a).size() == 1);
    CHECK(graph.edges_of(b).size() == 1);

    // 边没有 id（from/to 才是身份），所以按端点找回——与节点的 find(NodeId) 对称。
    CHECK(graph.find_edge(a, b) == e);
    CHECK(graph.find_edge(b, a) == nullptr);  // 有向：反向不是同一条
    CHECK(graph.find_edge(a, 99) == nullptr);
    const Graph& const_view = graph;
    CHECK(const_view.find_edge(a, b) == e);
}

TEST_CASE("graph: 删节点连带删边、删边") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, a);
    CHECK(graph.edge_count() == 2);

    CHECK_FALSE(graph.remove_node(99));
    CHECK(graph.remove_node(b));
    CHECK(graph.node_count() == 1);
    CHECK(graph.edge_count() == 0);  // 两条边都连到 b

    const NodeId c = graph.add_node("c").id();
    REQUIRE(graph.add_edge(a, c) != nullptr);
    CHECK(graph.remove_edge(a, c));
    CHECK_FALSE(graph.remove_edge(a, c));
}

TEST_CASE("graph: bounds 是节点外框的并集") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    CHECK(graph.bounds().w == doctest::Approx(0.0));  // 未布局

    graph.find(a)->set_box(Rect{0, 0, 6, 3});
    graph.find(b)->set_box(Rect{10, 5, 4, 3});
    const Rect r = graph.bounds();
    CHECK(r.x == doctest::Approx(0.0));
    CHECK(r.y == doctest::Approx(0.0));
    CHECK(r.w == doctest::Approx(14.0));
    CHECK(r.h == doctest::Approx(8.0));
}

TEST_CASE("graph/port: 锚点落在边框格上、按格索引取中点") {
    const Rect box{0, 0, 10, 4};  // 列 0..9，行 0..3

    const Point north = port_anchor(box, PortSide::North);
    CHECK(north.x == doctest::Approx(4.0));  // floor(9 * 0.5)
    CHECK(north.y == doctest::Approx(0.0));  // 上边框

    const Point east = port_anchor(box, PortSide::East);
    CHECK(east.x == doctest::Approx(9.0));   // 右边框
    CHECK(east.y == doctest::Approx(1.0));   // floor(3 * 0.5)

    const Point west_end = port_anchor(box, PortSide::West, 1.0);
    CHECK(west_end.x == doctest::Approx(0.0));
    CHECK(west_end.y == doctest::Approx(3.0));  // 该侧终点格

    const Point south_clamped = port_anchor(box, PortSide::South, 2.0);  // t 被夹取
    CHECK(south_clamped.x == doctest::Approx(9.0));
    CHECK(south_clamped.y == doctest::Approx(3.0));  // bottom() - 1

    CHECK(opposite(PortSide::North) == PortSide::South);
    CHECK(opposite(PortSide::East) == PortSide::West);
    CHECK(side_towards(Point{0, 0}, Point{10, 1}) == PortSide::East);
    CHECK(side_towards(Point{0, 0}, Point{-10, 0}) == PortSide::West);
    CHECK(side_towards(Point{0, 0}, Point{0, 10}) == PortSide::South);
    CHECK(side_towards(Point{0, 0}, Point{1, -10}) == PortSide::North);
}
