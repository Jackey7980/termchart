// test_diagrams_flow.cpp —— 三种图渲染（流程图 / 状态机图 / 思维导图）
#include <doctest.h>

#include <cstdint>

#include "termchart/canvas/canvas.hpp"
#include "termchart/graph/diagrams/flowchart.hpp"
#include "termchart/graph/diagrams/mindmap.hpp"
#include "termchart/graph/diagrams/state_machine.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/force.hpp"

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

// 箭头四个方向字符。
bool has_arrow(const canvas::Canvas& canvas) {
    return has_char(canvas, U'\u25B6') || has_char(canvas, U'\u25C0') ||
           has_char(canvas, U'\u25B2') || has_char(canvas, U'\u25BC');
}

// 某一行上是否连续出现给定 ASCII 文本（用于确认节点标签没被涂掉）。
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

double first_column_of(const canvas::Canvas& canvas, char32_t cp) {
    for (std::uint32_t x = 0; x < canvas.width(); ++x) {
        for (std::uint32_t y = 0; y < canvas.height(); ++y) {
            if (canvas.at(x, y).cp == cp) {
                return static_cast<double>(x);
            }
        }
    }
    return -1.0;
}

}  // namespace

TEST_CASE("diagrams/flowchart: 边框与箭头（回归）") {
    Graph graph;
    const NodeId a = graph.add_node("aaa").id();
    const NodeId b = graph.add_node("bbb").id();
    (void)graph.add_edge(a, b);

    canvas::Canvas canvas(40, 12);
    const Rect bounds = render_flowchart(graph, canvas, FlowchartOptions{});
    CHECK(bounds.w > 0.0);
    CHECK(has_char(canvas, U'\u250C'));  // 直角框
    CHECK(has_arrow(canvas));
    CHECK(has_char(canvas, U'a'));
}

TEST_CASE("diagrams/state_machine: 圆角状态 + 转移标签 + 起止标记") {
    Graph graph;
    const NodeId idle = graph.add_node("idle").id();
    const NodeId run = graph.add_node("run").id();
    graph.find(idle)->attributes().set("initial", "1");
    graph.find(run)->attributes().set("final", "1");
    (void)graph.add_edge(idle, run, "go");

    canvas::Canvas canvas(56, 12);
    StateMachineOptions opts;
    opts.layout.h_gap = 10.0;  // 给"go"留位置
    const Rect bounds = render_state_machine(graph, canvas, opts);

    CHECK(bounds.w > 0.0);
    CHECK(has_char(canvas, U'\u256D'));  // 圆角框
    CHECK(has_arrow(canvas));
    CHECK(has_char(canvas, U'\u25CF'));  // ● 初始标记
    CHECK(has_char(canvas, U'\u25CE'));  // ◎ 终止标记
    CHECK(has_char(canvas, U'g'));       // 转移事件名落到画布上
    CHECK(has_char(canvas, U'o'));

    // 关键不变量：边标签绝不能涂掉节点标签（放不下时宁可不画）。
    CHECK(row_contains(canvas, "idle"));
    CHECK(row_contains(canvas, "run"));
}

TEST_CASE("diagrams/state_machine: 空间不足时放弃边标签而不是覆盖节点标签") {
    // 纵向链 + 长转移名：标签候选位会被节点框吃掉，只能放弃绘制。
    Graph graph;
    const NodeId a = graph.add_node("running").id();
    const NodeId b = graph.add_node("error").id();
    (void)graph.add_edge(a, b, "verylonglabel");

    canvas::Canvas canvas(40, 14);
    StateMachineOptions opts;
    opts.layout.min_width = 9.0;
    (void)render_state_machine(graph, canvas, opts);

    CHECK(row_contains(canvas, "running"));
    CHECK(row_contains(canvas, "error"));
}

TEST_CASE("diagrams/state_machine: 关掉标记后不再画起止符号") {
    Graph graph;
    const NodeId idle = graph.add_node("idle").id();
    const NodeId run = graph.add_node("run").id();
    graph.find(idle)->attributes().set("initial", "1");
    graph.find(run)->attributes().set("final", "1");
    (void)graph.add_edge(idle, run);

    canvas::Canvas canvas(56, 12);
    StateMachineOptions opts;
    opts.mark_initial_final = false;
    (void)render_state_machine(graph, canvas, opts);
    CHECK_FALSE(has_char(canvas, U'\u25CF'));
    CHECK_FALSE(has_char(canvas, U'\u25CE'));
}

TEST_CASE("diagrams/mindmap: 根居中、分支分列两侧、分支不带箭头") {
    Graph graph;
    const NodeId center = graph.add_node("center").id();
    const NodeId right = graph.add_node("R").id();
    const NodeId left = graph.add_node("L").id();
    (void)graph.add_edge(center, right);
    (void)graph.add_edge(center, left);

    canvas::Canvas canvas(60, 12);
    const Rect bounds = render_mindmap(graph, canvas, MindmapOptions{});
    CHECK(bounds.w > 0.0);
    CHECK_FALSE(has_arrow(canvas));  // 思维导图分支没有箭头

    const double center_col = first_column_of(canvas, U'c');
    const double right_col = first_column_of(canvas, U'R');
    const double left_col = first_column_of(canvas, U'L');
    REQUIRE(center_col >= 0.0);
    REQUIRE(right_col >= 0.0);
    REQUIRE(left_col >= 0.0);
    CHECK(left_col < center_col);
    CHECK(right_col > center_col);
}

TEST_CASE("diagrams/node_link: 力导向布局后可渲染且节点不重叠") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    const NodeId c = graph.add_node("c").id();
    const NodeId d = graph.add_node("d").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, c);
    (void)graph.add_edge(c, d);
    (void)graph.add_edge(d, a);

    ForceOptions force;
    force.layout.min_width = 8.0;
    force.ideal_edge_length = 8.0;
    (void)layout_force(graph, force);

    canvas::Canvas canvas(70, 20);
    NodeLinkOptions opts;
    opts.route_style = RouteStyle::Straight;
    opts.node_border = true;
    render_node_link(graph, canvas, opts);

    CHECK(has_char(canvas, U'\u250C'));  // 边框可见
    CHECK(has_char(canvas, U'a'));
    CHECK_FALSE(has_arrow(canvas));      // 默认无箭头

    for (std::size_t i = 0; i < graph.node_count(); ++i) {
        for (std::size_t j = i + 1; j < graph.node_count(); ++j) {
            const Rect p = graph.nodes()[i].box();
            const Rect q = graph.nodes()[j].box();
            const bool overlap =
                p.x < q.right() && q.x < p.right() && p.y < q.bottom() && q.y < p.bottom();
            CHECK_FALSE(overlap);
        }
    }
}
