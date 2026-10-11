// test_graph_route.cpp —— 直线与正交路由、栅格避让
#include <doctest.h>

#include <cmath>
#include <cstddef>
#include <vector>

#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"
#include "termchart/graph/route/route.hpp"
#include "termchart/graph/wiregrid.hpp"

namespace {

using namespace termchart;
using namespace termchart::graph;

}  // namespace

TEST_CASE("graph/route: 直线路由端点落在两侧边框上") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    (void)graph.add_edge(a, b);
    (void)layout_layered(graph, LayoutOptions{});

    CHECK(route_edges(graph, RouteStyle::Straight) == 1);
    const std::vector<Point>& route = graph.edges()[0].route();
    REQUIRE(route.size() == 2);

    const Rect ab = graph.find(a)->box();
    const Rect bb = graph.find(b)->box();
    // 起点在 a 的东边框列上，终点在 b 的西边框列上，且都吸附到整数格。
    CHECK(route[0].x == doctest::Approx(ab.right() - 1.0));
    CHECK(route[0].y >= doctest::Approx(ab.y));
    CHECK(route[0].y <= doctest::Approx(ab.bottom()));
    CHECK(route[0].y == doctest::Approx(std::round(route[0].y)));
    CHECK(route[1].x == doctest::Approx(bb.x));
    CHECK(route[1].y >= doctest::Approx(bb.y));
    CHECK(route[1].y <= doctest::Approx(bb.bottom()));
    CHECK(route[1].y == doctest::Approx(std::round(route[1].y)));
}

TEST_CASE("graph/route: 正交路由是轴对齐折线并占用栅格") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    (void)graph.add_edge(a, b);
    (void)layout_layered(graph, LayoutOptions{});

    WireGrid grid(60, 20);
    CHECK(route_edges(graph, RouteStyle::Orthogonal, RouteOptions{}, &grid) == 1);

    // 同行端点会退化为一条轴对齐水平段（共线中间点已被简化掉）。
    const std::vector<Point>& route = graph.edges()[0].route();
    CHECK(route.size() >= 2);
    for (std::size_t i = 1; i < route.size(); ++i) {
        const bool axis_aligned =
            route[i].x == route[i - 1].x || route[i].y == route[i - 1].y;
        CHECK(axis_aligned);
    }
    CHECK(grid.occupied_count() > 0);
}

TEST_CASE("graph/route: 同侧扇出的多条边各自成 Z 形") {
    Graph graph;
    const NodeId root = graph.add_node("root").id();
    const NodeId left = graph.add_node("left").id();
    const NodeId right = graph.add_node("right").id();
    (void)graph.add_edge(root, left);
    (void)graph.add_edge(root, right);
    (void)layout_layered(graph, LayoutOptions{});

    WireGrid grid(60, 20);
    CHECK(route_edges(graph, RouteStyle::Orthogonal, RouteOptions{}, &grid) == 2);

    // 两条边都从 root 东侧扇出，锚点被分到不同行；折线全程轴对齐、坐标落在整数格。
    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        REQUIRE(route.size() >= 2);
        for (const Point& p : route) {
            CHECK(p.x == doctest::Approx(std::round(p.x)));
            CHECK(p.y == doctest::Approx(std::round(p.y)));
        }
        for (std::size_t i = 1; i < route.size(); ++i) {
            const bool axis_aligned =
                route[i].x == route[i - 1].x || route[i].y == route[i - 1].y;
            CHECK(axis_aligned);
        }
    }
    CHECK(grid.occupied_count() > 0);
}

TEST_CASE("graph/route: 正交路由的中间格不落在任何节点框内") {
    Graph graph;
    const NodeId root = graph.add_node("root").id();
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    const NodeId leaf = graph.add_node("leaf").id();
    (void)graph.add_edge(root, a);
    (void)graph.add_edge(root, b);
    (void)graph.add_edge(a, leaf);
    (void)graph.add_edge(b, leaf);
    (void)layout_layered(graph, LayoutOptions{});

    WireGrid grid(80, 24);
    CHECK(route_edges(graph, RouteStyle::Orthogonal, RouteOptions{}, &grid) == 4);

    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        std::vector<Point> cells;
        for (std::size_t i = 1; i < route.size(); ++i) {
            const double steps =
                std::max(std::fabs(route[i].x - route[i - 1].x),
                         std::fabs(route[i].y - route[i - 1].y));
            const int count = static_cast<int>(steps);
            for (int k = 0; k <= count; ++k) {
                const double t = (count == 0) ? 0.0 : static_cast<double>(k) / count;
                cells.push_back(Point{route[i - 1].x + (route[i].x - route[i - 1].x) * t,
                                      route[i - 1].y + (route[i].y - route[i - 1].y) * t});
            }
        }
        if (cells.size() <= 2) {
            continue;
        }
        // 首末格是节点边框上的锚点，跳过；其余中间格不得落进任何节点框。
        for (std::size_t c = 1; c + 1 < cells.size(); ++c) {
            for (const Node& node : graph.nodes()) {
                const Rect box = node.box();
                const bool inside = cells[c].x >= box.x && cells[c].x < box.right() &&
                                    cells[c].y >= box.y && cells[c].y < box.bottom();
                CHECK_MESSAGE(!inside, "cell (", cells[c].x, ",", cells[c].y,
                              ") inside node box (", box.x, ",", box.y, ",", box.w, ",",
                              box.h, ")");
            }
        }
    }
}

TEST_CASE("graph/route: 分层流程图的边全部轴对齐") {
    Graph graph;
    const NodeId start = graph.add_node("start").id();
    const NodeId parse = graph.add_node("parse").id();
    const NodeId check = graph.add_node("valid?").id();
    const NodeId work = graph.add_node("process").id();
    const NodeId fail = graph.add_node("error").id();
    const NodeId done = graph.add_node("done").id();
    (void)graph.add_edge(start, parse);
    (void)graph.add_edge(parse, check);
    (void)graph.add_edge(check, work);
    (void)graph.add_edge(check, fail);
    (void)graph.add_edge(work, done);
    (void)graph.add_edge(fail, done);

    LayoutOptions opts;
    opts.min_width = 8.0;
    opts.h_gap = 3.0;
    (void)layout_layered(graph, opts);

    WireGrid grid(64, 18);
    CHECK(route_edges(graph, RouteStyle::Orthogonal, RouteOptions{}, &grid) == 6);
    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        CHECK_MESSAGE(route.size() >= 2, "edge ", edge.from(), "->", edge.to(), " size ",
                      route.size());
        for (std::size_t i = 1; i < route.size(); ++i) {
            CHECK_MESSAGE((route[i].x == route[i - 1].x || route[i].y == route[i - 1].y),
                          "edge ", edge.from(), "->", edge.to(), " size ", route.size(),
                          " seg ", i, ": (", route[i - 1].x, ",", route[i - 1].y, ") -> (",
                          route[i].x, ",", route[i].y, ")");
        }
    }
}

TEST_CASE("graph/route: 栅格避让使相交的两条边不重合") {
    Graph graph;
    const NodeId a1 = graph.add_node("a1").id();
    const NodeId a2 = graph.add_node("a2").id();
    const NodeId b1 = graph.add_node("b1").id();
    const NodeId b2 = graph.add_node("b2").id();
    (void)graph.add_edge(a1, b2);
    (void)graph.add_edge(a2, b1);
    (void)layout_layered(graph, LayoutOptions{});

    WireGrid grid(60, 20);
    CHECK(route_edges(graph, RouteStyle::Orthogonal, RouteOptions{}, &grid) == 2);
    CHECK(graph.edges()[0].route() != graph.edges()[1].route());
    CHECK(grid.occupied_count() > 0);
}
