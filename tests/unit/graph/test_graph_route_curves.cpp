// test_graph_route_curves.cpp —— 曲线边路由（spline / arc）的真不变量
//
// 这里守住的是四条不变量，而不是"长什么样"：
//   1. 端点严格等于锚点（首点=源锚点、末点=目标锚点，且在整数格上）；
//   2. 任何中间格都不落进任何节点框内部（半开区间判定）——包括"两框只剩 1 格空隙"
//      的挤压情形；
//   3. 相邻点 8 连通（|dx|<=1 && |dy|<=1 且不重复），曲线在格粒度上不许跳格；
//   4. 确定性：同一输入两次调用逐点相等。
// 自环另加一条：整圈绕在框外侧，且不覆盖节点标签所在格。
#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/route/arc.hpp"
#include "termchart/graph/route/route.hpp"
#include "termchart/graph/route/spline.hpp"
#include "termchart/graph/shape/shape.hpp"
#include "termchart/graph/wiregrid.hpp"

namespace {

using namespace termchart;
using namespace termchart::graph;

std::vector<Rect> node_boxes(const Graph& graph) {
    std::vector<Rect> boxes;
    for (const Node& node : graph.nodes()) {
        const Rect box = node.box();
        if (box.w >= 1.0 && box.h >= 1.0) {
            boxes.push_back(box);
        }
    }
    return boxes;
}

bool inside_rect(const Rect& box, Point p) {
    return p.x >= box.x && p.x < box.right() && p.y >= box.y && p.y < box.bottom();
}

bool inside_any(const std::vector<Rect>& boxes, Point p) {
    for (const Rect& box : boxes) {
        if (inside_rect(box, p)) {
            return true;
        }
    }
    return false;
}

// doctest 不接受把 `a && b && c` 直接写进 CHECK_MESSAGE（表达式分解限制），
// 所以这类复合判定都先落成一个 bool 再断言。
bool is_eight_connected_step(Point from, Point to) {
    const double dx = std::fabs(to.x - from.x);
    const double dy = std::fabs(to.y - from.y);
    return dx <= 1.0 && dy <= 1.0 && (dx + dy) >= 1.0;
}

// 不变量 3：相邻点 8 连通，且不出现零位移的重复点。
void check_eight_connected(const std::vector<Point>& route) {
    for (std::size_t i = 1; i < route.size(); ++i) {
        CHECK_MESSAGE(is_eight_connected_step(route[i - 1], route[i]), "step ", i, ": (",
                      route[i - 1].x, ",", route[i - 1].y, ") -> (", route[i].x, ",", route[i].y,
                      ")");
    }
}

// 不变量 2：首末格是节点边框上的锚点（节点框自身也是障碍），跳过；其余格不得进框。
void check_no_box_crossing(const Graph& graph) {
    const std::vector<Rect> boxes = node_boxes(graph);
    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        if (route.size() <= 2) {
            continue;
        }
        for (std::size_t i = 1; i + 1 < route.size(); ++i) {
            const bool outside = !inside_any(boxes, route[i]);
            CHECK_MESSAGE(outside, "edge ", edge.from(), "->", edge.to(), " cell (", route[i].x,
                          ",", route[i].y, ") 落在某个节点框内");
        }
    }
}

void check_endpoints(const Graph& graph, const std::vector<Point>& before) {
    const std::vector<Point>& route = graph.edges()[0].route();
    REQUIRE(route.size() >= 2);
    CHECK(route.front().x == before.front().x);
    CHECK(route.front().y == before.front().y);
    CHECK(route.back().x == before.back().x);
    CHECK(route.back().y == before.back().y);
}

bool has_diagonal_step(const std::vector<Point>& route) {
    for (std::size_t i = 1; i < route.size(); ++i) {
        if (std::fabs(route[i].x - route[i - 1].x) == 1.0 &&
            std::fabs(route[i].y - route[i - 1].y) == 1.0) {
            return true;
        }
    }
    return false;
}

double max_x(const std::vector<Point>& route) {
    double m = -1.0e30;
    for (const Point& p : route) {
        m = std::max(m, p.x);
    }
    return m;
}

bool on_rect_border(const Rect& box, Point p) {
    const bool within_x = p.x >= box.x && p.x < box.right();
    const bool within_y = p.y >= box.y && p.y < box.bottom();
    const bool on_edge = p.x == box.x || p.x == box.right() - 1.0 || p.y == box.y ||
                         p.y == box.bottom() - 1.0;
    return within_x && within_y && on_edge;
}

// 记录 anchor 折线（route_edges 的直线风格结果恰是 [from_anchor, to_anchor]）。
std::vector<Point> anchor_route(Graph& graph) {
    (void)route_edges(graph, RouteStyle::Straight, RouteOptions{}, nullptr);
    return graph.edges()[0].route();
}

}  // namespace

// ── spline ────────────────────────────────────────────────────────────────────

TEST_CASE("graph/route/spline: 端点严格等于锚点且曲线 8 连通") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    graph.find(a)->set_box(Rect{0.0, 0.0, 5.0, 5.0});
    graph.find(b)->set_box(Rect{21.0, 10.0, 5.0, 5.0});
    (void)graph.add_edge(a, b);

    const std::vector<Point> before = anchor_route(graph);
    WireGrid grid(40, 20);
    CHECK(route_spline(graph, RouteOptions{}, &grid) == 1);

    check_endpoints(graph, before);
    check_eight_connected(graph.edges()[0].route());
    check_no_box_crossing(graph);
}

TEST_CASE("graph/route/spline: 转弯段确实产生斜向格（曲线而非折线）") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    // 长横距 + 纵向偏移：控制多边形有足够"弦长"让 Catmull-Rom 把两个直角抹圆，
    // 抹圆处必然出现 ╱ ╲ 对应的 1 格对角步。
    graph.find(a)->set_box(Rect{0.0, 0.0, 5.0, 5.0});
    graph.find(b)->set_box(Rect{21.0, 10.0, 5.0, 5.0});
    (void)graph.add_edge(a, b);

    WireGrid grid(40, 20);
    CHECK(route_spline(graph, RouteOptions{}, &grid) == 1);
    const std::vector<Point>& route = graph.edges()[0].route();
    CHECK(has_diagonal_step(route));
    // 曲线还应当偏离首尾锚点所在的直线（否则只是把直连又画了一遍）。
    const Point a0 = route.front();
    const Point b0 = route.back();
    bool deviated = false;
    for (const Point& p : route) {
        const double t = (b0.x == a0.x) ? 0.0 : (p.x - a0.x) / (b0.x - a0.x);
        if (t > 0.0 && t < 1.0) {
            const double on_chord = a0.y + (b0.y - a0.y) * t;
            if (std::fabs(p.y - on_chord) >= 1.0) {
                deviated = true;
            }
        }
    }
    CHECK(deviated);
}

TEST_CASE("graph/route/spline: 两框只剩 1 格空隙时不穿框且走那 1 格") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    // a 占 x∈[0,4]，b 占 x∈[6,10]：x=5 是唯一的空档列。
    graph.find(a)->set_box(Rect{0.0, 0.0, 5.0, 5.0});
    graph.find(b)->set_box(Rect{6.0, 0.0, 5.0, 5.0});
    (void)graph.add_edge(a, b);

    const std::vector<Point> before = anchor_route(graph);
    WireGrid grid(40, 16);
    CHECK(route_spline(graph, RouteOptions{}, &grid) == 1);

    const std::vector<Point>& route = graph.edges()[0].route();
    check_endpoints(graph, before);
    check_eight_connected(route);
    check_no_box_crossing(graph);
    const bool used_gap = std::any_of(route.begin(), route.end(),
                                      [](const Point& p) { return p.x == 5.0; });
    CHECK(used_gap);
}

TEST_CASE("graph/route/spline: 不给栅格也会自建障碍并避开节点") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    graph.find(a)->set_box(Rect{0.0, 0.0, 5.0, 5.0});
    graph.find(b)->set_box(Rect{6.0, 0.0, 5.0, 5.0});
    (void)graph.add_edge(a, b);

    CHECK(route_spline(graph) == 1);  // grid == nullptr
    check_no_box_crossing(graph);
    check_eight_connected(graph.edges()[0].route());
}

TEST_CASE("graph/route/spline: 同一输入两次调用逐点相等") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    graph.find(a)->set_box(Rect{0.0, 0.0, 5.0, 5.0});
    graph.find(b)->set_box(Rect{21.0, 10.0, 5.0, 5.0});
    (void)graph.add_edge(a, b);

    WireGrid grid1(40, 20);
    WireGrid grid2(40, 20);
    CHECK(route_spline(graph, RouteOptions{}, &grid1) == 1);
    const std::vector<Point> first = graph.edges()[0].route();
    CHECK(route_spline(graph, RouteOptions{}, &grid2) == 1);
    CHECK(graph.edges()[0].route() == first);
}

TEST_CASE("graph/route/spline: 空图 / 无边 / 未布局都不崩且行为明确") {
    Graph empty;
    CHECK(route_spline(empty) == 0);

    Graph single;
    (void)single.add_node("only");
    CHECK(route_spline(single) == 0);

    // 未布局：节点外框还是默认空矩形，锚点退化到同一格。约定"非空"（单点），不崩。
    Graph unlaid;
    const NodeId a = unlaid.add_node("a").id();
    const NodeId b = unlaid.add_node("b").id();
    (void)unlaid.add_edge(a, b);
    CHECK(route_spline(unlaid) == 1);
    CHECK(unlaid.edges()[0].route().size() >= 1);
}

// ── arc ───────────────────────────────────────────────────────────────────────

TEST_CASE("graph/route/arc: 跨列弧线端点严格等于锚点且不穿框") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    graph.find(a)->set_box(Rect{0.0, 0.0, 5.0, 5.0});
    graph.find(b)->set_box(Rect{6.0, 0.0, 5.0, 5.0});
    (void)graph.add_edge(a, b);

    const std::vector<Point> before = anchor_route(graph);
    WireGrid grid(40, 16);
    CHECK(route_arc(graph, RouteOptions{}, &grid) == 1);

    const std::vector<Point>& route = graph.edges()[0].route();
    check_endpoints(graph, before);
    check_eight_connected(route);
    check_no_box_crossing(graph);
}

TEST_CASE("graph/route/arc: 自环绕在节点框外侧且不覆盖标签") {
    Graph graph;
    const NodeId a = graph.add_node("loop").id();
    const Rect box{2.0, 2.0, 9.0, 3.0};
    graph.find(a)->set_box(box);
    (void)graph.add_edge(a, a);

    WireGrid grid(40, 16);
    CHECK(route_arc(graph, RouteOptions{}, &grid) == 1);

    const std::vector<Point>& route = graph.edges()[0].route();
    REQUIRE(route.size() >= 2);
    check_eight_connected(route);
    // 首末点都在同一个节点的边框上（构成回到该节点的回路）。
    CHECK(on_rect_border(box, route.front()));
    CHECK(on_rect_border(box, route.back()));
    // 绕到框外：至少有一格走到节点右边界之外。
    CHECK(max_x(route) >= box.right());
    // 任何中间格都不在框内。
    for (std::size_t i = 1; i + 1 < route.size(); ++i) {
        const bool outside = !inside_rect(box, route[i]);
        CHECK_MESSAGE(outside, "自环中间格 (", route[i].x, ",", route[i].y, ") 落在框内");
    }

    // 标签画在框内居中处；自环整圈在框外，因此标签格不应被改写。
    canvas::Canvas canvas(40, 16);
    draw_shape(canvas, ShapeKind::Box, box, Style{});
    const int label_width = 4;
    const double label_x = box.x + std::max(0.0, (box.w - static_cast<double>(label_width)) / 2.0);
    const double label_y = box.y + std::floor(std::max(0.0, box.h - 1.0) / 2.0);
    canvas.draw_text(Point{label_x, label_y}, "loop", Style{});

    const auto label_col = static_cast<std::uint32_t>(std::floor(label_x));
    const auto label_row = static_cast<std::uint32_t>(std::floor(label_y));
    std::vector<char32_t> before_label;
    for (int i = 0; i < label_width; ++i) {
        before_label.push_back(canvas.at(label_col + static_cast<std::uint32_t>(i), label_row).cp);
    }
    CHECK(before_label.front() == U'l');

    for (const Point& p : route) {
        canvas.put(p, U'#', Style{});
    }
    for (int i = 0; i < label_width; ++i) {
        CHECK(canvas.at(label_col + static_cast<std::uint32_t>(i), label_row).cp ==
              before_label[static_cast<std::size_t>(i)]);
    }
}

TEST_CASE("graph/route/arc: 同一输入两次调用逐点相等") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    graph.find(a)->set_box(Rect{0.0, 0.0, 5.0, 5.0});
    graph.find(b)->set_box(Rect{6.0, 6.0, 5.0, 5.0});
    (void)graph.add_edge(a, b);

    WireGrid grid1(40, 20);
    WireGrid grid2(40, 20);
    CHECK(route_arc(graph, RouteOptions{}, &grid1) == 1);
    const std::vector<Point> first = graph.edges()[0].route();
    CHECK(route_arc(graph, RouteOptions{}, &grid2) == 1);
    CHECK(graph.edges()[0].route() == first);
}

TEST_CASE("graph/route/arc: 空图 / 单节点 / 极端画布 / 退化框都不崩") {
    Graph empty;
    CHECK(route_arc(empty) == 0);

    Graph single;
    (void)single.add_node("only");
    CHECK(route_arc(single) == 0);

    // 画布只有 1 格：所有候选都放不下，约定退化为直线且不为空。
    Graph cramped;
    const NodeId a = cramped.add_node("a").id();
    const NodeId b = cramped.add_node("b").id();
    cramped.find(a)->set_box(Rect{0.0, 0.0, 5.0, 5.0});
    cramped.find(b)->set_box(Rect{6.0, 0.0, 5.0, 5.0});
    (void)cramped.add_edge(a, b);
    WireGrid tiny(1, 1);
    CHECK(route_arc(cramped, RouteOptions{}, &tiny) == 1);
    CHECK(cramped.edges()[0].route().size() >= 1);

    // 1×1 的框：没有外侧空间，约定退化为单点，不崩。
    Graph dot;
    const NodeId d = dot.add_node("d").id();
    dot.find(d)->set_box(Rect{3.0, 3.0, 1.0, 1.0});
    (void)dot.add_edge(d, d);
    CHECK(route_arc(dot) == 1);
    CHECK(dot.edges()[0].route().size() >= 1);

    // 两个节点被摆在同一个框上：锚点重合，约定退化为单点，不崩。
    Graph overlap;
    const NodeId p = overlap.add_node("p").id();
    const NodeId q = overlap.add_node("q").id();
    overlap.find(p)->set_box(Rect{4.0, 4.0, 5.0, 5.0});
    overlap.find(q)->set_box(Rect{4.0, 4.0, 5.0, 5.0});
    (void)overlap.add_edge(p, q);
    CHECK(route_arc(overlap) == 1);
    CHECK(overlap.edges()[0].route().size() >= 1);
}
