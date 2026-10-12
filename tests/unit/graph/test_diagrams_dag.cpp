// test_diagrams_dag.cpp —— DAG 与依赖图渲染入口
#include <doctest.h>

#include <cstdint>

#include "termchart/canvas/canvas.hpp"
#include "termchart/graph/diagrams/dependency.hpp"
#include "termchart/graph/diagrams/dag.hpp"
#include "termchart/graph/graph.hpp"

namespace {

using namespace termchart;
using namespace termchart::graph;

bool has_char(const canvas::Canvas& canvas, char32_t cp) {
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            if (canvas.at(x, y).cp == cp) {
                return true;
            }
        }
    }
    return false;
}

bool has_arrow(const canvas::Canvas& canvas) {
    return has_char(canvas, U'\u25B6') || has_char(canvas, U'\u25C0') ||
           has_char(canvas, U'\u25B2') || has_char(canvas, U'\u25BC');
}

// 某一行上是否连续出现给定 ASCII 文本（确认节点标签没被涂掉）。
bool row_contains(const canvas::Canvas& canvas, const std::string& text) {
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x + text.size() <= canvas.width(); ++x) {
            bool match = true;
            for (std::size_t k = 0; k < text.size(); ++k) {
                if (canvas.at(x + static_cast<std::uint32_t>(k), y).cp !=
                    static_cast<char32_t>(text[k])) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

TEST_CASE("graph/is_acyclic: 链是无环，三角与自环是环") {
    Graph chain;
    const NodeId a = chain.add_node("a").id();
    const NodeId b = chain.add_node("b").id();
    const NodeId c = chain.add_node("c").id();
    (void)chain.add_edge(a, b);
    (void)chain.add_edge(b, c);
    CHECK(is_acyclic(chain));

    Graph cycle;
    const NodeId x = cycle.add_node("x").id();
    const NodeId y = cycle.add_node("y").id();
    const NodeId z = cycle.add_node("z").id();
    (void)cycle.add_edge(x, y);
    (void)cycle.add_edge(y, z);
    (void)cycle.add_edge(z, x);
    CHECK_FALSE(is_acyclic(cycle));

    Graph loop;
    const NodeId s = loop.add_node("s").id();
    (void)loop.add_edge(s, s);
    CHECK_FALSE(is_acyclic(loop));

    Graph empty;
    CHECK(is_acyclic(empty));
}

TEST_CASE("diagrams/dag: 无环图渲染出边框与箭头") {
    Graph graph;
    const NodeId a = graph.add_node("aaa").id();
    const NodeId b = graph.add_node("bbb").id();
    (void)graph.add_edge(a, b);

    canvas::Canvas canvas(40, 12);
    const Rect bounds = render_dag(graph, canvas, DagOptions{});
    CHECK(bounds.w > 0.0);
    CHECK(has_char(canvas, U'\u250C'));  // 直角框
    CHECK(has_arrow(canvas));
    CHECK(row_contains(canvas, "aaa"));
    CHECK(row_contains(canvas, "bbb"));
}

TEST_CASE("diagrams/dag: 有环时不崩溃、不丢节点、标签完好") {
    Graph graph;
    const NodeId a = graph.add_node("alpha").id();
    const NodeId b = graph.add_node("beta").id();
    const NodeId c = graph.add_node("gamma").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, c);
    (void)graph.add_edge(c, a);  // 回边

    canvas::Canvas canvas(60, 20);
    CHECK_FALSE(is_acyclic(graph));
    const Rect bounds = render_dag(graph, canvas, DagOptions{});
    CHECK(bounds.w > 0.0);
    CHECK(row_contains(canvas, "alpha"));
    CHECK(row_contains(canvas, "beta"));
    CHECK(row_contains(canvas, "gamma"));
}

TEST_CASE("diagrams/dependency: 无环时边保持终点箭头") {
    Graph graph;
    const NodeId a = graph.add_node("app").id();
    const NodeId b = graph.add_node("lib").id();
    const NodeId c = graph.add_node("core").id();
    (void)graph.add_edge(a, b);  // app 依赖 lib
    (void)graph.add_edge(b, c);  // lib 依赖 core

    canvas::Canvas canvas(60, 12);
    const Rect bounds = render_dependency(graph, canvas, DependencyOptions{});
    CHECK(bounds.w > 0.0);
    CHECK(has_arrow(canvas));
    // 无环：没有边被改成双向箭头。
    for (const Edge& e : graph.edges()) {
        CHECK(e.arrow() == ArrowStyle::AtTarget);
    }
    // 依赖方向一致：被依赖者排在更右的层（core 的框在最右）。
    CHECK(graph.find(c)->box().x > graph.find(a)->box().x);
}

TEST_CASE("diagrams/dependency: 环上的边被标记为双向箭头") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    const NodeId c = graph.add_node("c").id();
    const NodeId d = graph.add_node("d").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, c);
    (void)graph.add_edge(c, a);  // a→b→c→a 是环
    (void)graph.add_edge(d, a);  // d 不在环上

    canvas::Canvas canvas(70, 20);
    (void)render_dependency(graph, canvas, DependencyOptions{});

    // 环上的边（a→b、b→c、c→a）被改写成两端箭头；环外的 d→a 保持原样。
    for (const Edge& e : graph.edges()) {
        const bool on_cycle = (e.from() == a && e.to() == b) ||
                              (e.from() == b && e.to() == c) ||
                              (e.from() == c && e.to() == a);
        if (on_cycle) {
            CHECK(e.arrow() == ArrowStyle::Both);
        } else {
            CHECK(e.arrow() == ArrowStyle::AtTarget);
        }
    }
}