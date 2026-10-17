// test_graph_route_obstacle_fix.cpp —— route_avoiding 不穿障契约回归（P4）
//
// 缺陷背景：route_edges 的单折点 Z 形路由在障碍横挡直连走廊、或 Z 形起始段贴
// 着源节点框边框列走时全部候选被否决，退化成直线兜底——直线不做占用检查，
// 直接穿过障碍。修复后 route_avoiding 对兜底直线做 BFS 栅格重绕，本文件锁住
// "中间格不得进入外扩障碍或任何节点框"的严格断言。
#include <doctest.h>

#include <cmath>
#include <cstddef>
#include <vector>

#include "termchart/graph/graph.hpp"
#include "termchart/graph/route/obstacle.hpp"

namespace {

using namespace termchart;
using namespace termchart::graph;

Rect expanded(const Rect& r, double c) {
    return Rect{r.x - c, r.y - c, r.w + 2.0 * c, r.h + 2.0 * c};
}

// 折线展开成逐格点列（轴对齐段逐格，斜线段按最大轴向步数采样）。
std::vector<Point> route_cells(const std::vector<Point>& route) {
    std::vector<Point> cells;
    for (std::size_t i = 1; i < route.size(); ++i) {
        const Point& p0 = route[i - 1];
        const Point& p1 = route[i];
        const double span = std::max(std::fabs(p1.x - p0.x), std::fabs(p1.y - p0.y));
        const auto steps = static_cast<int>(std::ceil(span));
        for (int s = 0; s <= steps; ++s) {
            const double t = static_cast<double>(s) / static_cast<double>(steps);
            cells.push_back(Point{std::round(p0.x + (p1.x - p0.x) * t),
                                  std::round(p0.y + (p1.y - p0.y) * t)});
        }
    }
    return cells;
}

// 严格断言：除首末锚点格外，折线不得进入任何禁行区（外扩障碍 / 节点框）。
void check_route_avoids(const Graph& graph, const std::vector<Rect>& zones) {
    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        REQUIRE(route.size() >= 2);
        // 折线全程轴对齐（兜底直线被 BFS 重绕替换后必然满足）。
        for (std::size_t i = 1; i < route.size(); ++i) {
            CHECK_MESSAGE((route[i].x == route[i - 1].x || route[i].y == route[i - 1].y),
                          "edge ", edge.from(), "->", edge.to(), " seg ", i,
                          " is not axis-aligned");
        }
        const std::vector<Point> cells = route_cells(route);
        if (cells.size() <= 2) {
            continue;
        }
        for (std::size_t c = 1; c + 1 < cells.size(); ++c) {
            for (std::size_t z = 0; z < zones.size(); ++z) {
                const Rect& zone = zones[z];
                const bool inside = cells[c].x >= zone.x && cells[c].x < zone.right() &&
                                    cells[c].y >= zone.y && cells[c].y < zone.bottom();
                CHECK_MESSAGE(!inside, "cell (", cells[c].x, ",", cells[c].y,
                              ") of edge ", edge.from(), "->", edge.to(),
                              " enters forbidden zone #", z);
            }
        }
    }
}

}  // namespace

TEST_CASE("graph/route_avoiding: 障碍横挡直连走廊时折线不穿障") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    // 手工摆位：左上 → 右下，横挡正好挡在直连走廊上。
    graph.find(a)->set_box(Rect{1.0, 1.0, 8.0, 4.0});
    graph.find(b)->set_box(Rect{20.0, 10.0, 8.0, 4.0});
    (void)graph.add_edge(a, b);

    const Rect obstacle{10.0, 5.0, 12.0, 3.0};
    ObstacleOptions opts;
    opts.route.max_detour = 16;
    CHECK(route_avoiding(graph, {obstacle}, opts) == 1);

    const Rect ab = graph.find(a)->box();
    const Rect bb = graph.find(b)->box();
    const std::vector<Point>& route = graph.edges()[0].route();
    CHECK(route.front().x >= ab.x);
    CHECK(route.front().x <= ab.right());
    CHECK(route.back().x >= bb.x);
    CHECK(route.back().x <= bb.right());

    // 禁行区 = 外扩障碍 + 两个节点框；中间格一律不得进入。
    const std::vector<Rect> zones{expanded(obstacle, 1.0), ab, bb};
    check_route_avoids(graph, zones);
    CHECK_FALSE(graph.edges()[0].route_failed());  // 成功重绕 → 非失败兜底（雷区1）
}

TEST_CASE("graph/route_avoiding: 修复需绕整屏时放弃并保留直连兜底（雷区1+雷区3）") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    // a 在上、b 在下，同列。一条横贯全宽的墙（y=5..6 + clearance 1 → y=4..7）堵死直连，
    // 唯一空通道是绕到栅格右端（x≈300）再折返——远超修复预算，BFS 应放弃，
    // 保留 2 点直连 best-effort（route_failed 为真），且绝不产出绕整屏的巨凹形。
    graph.find(a)->set_box(Rect{1.0, 1.0, 3.0, 2.0});
    graph.find(b)->set_box(Rect{1.0, 9.0, 3.0, 2.0});
    (void)graph.add_edge(a, b);

    const Rect obstacle{0.0, 5.0, 300.0, 1.0};
    ObstacleOptions opts;
    opts.route.max_detour = 4;
    CHECK(route_avoiding(graph, {obstacle}, opts) == 1);

    const Edge& edge = graph.edges()[0];
    CHECK(edge.route_failed());  // 正交失败 → 兜底直线，标记失败
    // 修复被预算截断：不得被重绕成绕整屏的巨型路径（雷区3·坑位B）。
    const std::vector<Point> cells = route_cells(edge.route());
    CHECK(cells.size() <= 50);
    // 仍把两端连上（锚点落在各自框上）。
    const Rect ab = graph.find(a)->box();
    const Rect bb = graph.find(b)->box();
    CHECK(edge.route().front().y >= ab.y);
    CHECK(edge.route().front().y <= ab.bottom());
    CHECK(edge.route().back().y >= bb.y);
    CHECK(edge.route().back().y <= bb.bottom());
}

TEST_CASE("graph/route_avoiding: Z 形起始段贴源框时兜底直线被重绕") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    // 源框在左上，目标框在右下且水平方向有交叠：离开侧被判为 East，Z 形的竖向
    // 起始段贴着源框东边框列走，所有 Z/L 候选被源框自身否决，旧实现退化成直线
    // 兜底并穿过横挡。
    graph.find(a)->set_box(Rect{1.0, 1.0, 10.0, 4.0});
    graph.find(b)->set_box(Rect{7.0, 6.0, 10.0, 4.0});
    (void)graph.add_edge(a, b);

    // 横挡夹在两框之间（含 clearance 1 后盖住 y=4..6、x=1..16 的走廊）。
    const Rect obstacle{2.0, 5.0, 14.0, 1.0};
    CHECK(route_avoiding(graph, {obstacle}, ObstacleOptions{}) == 1);

    const std::vector<Rect> zones{expanded(obstacle, 1.0), graph.find(a)->box(),
                                  graph.find(b)->box()};
    check_route_avoids(graph, zones);
}

TEST_CASE("graph/route_avoiding: 修复不改变锚点端点") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    graph.find(a)->set_box(Rect{1.0, 1.0, 10.0, 4.0});
    graph.find(b)->set_box(Rect{7.0, 6.0, 10.0, 4.0});
    (void)graph.add_edge(a, b);

    const Rect obstacle{2.0, 5.0, 14.0, 1.0};
    (void)route_avoiding(graph, {obstacle}, ObstacleOptions{});

    // 重绕只改中间段：首末点仍是两条边框上的锚点。
    const std::vector<Point>& route = graph.edges()[0].route();
    const Rect ab = graph.find(a)->box();
    const Rect bb = graph.find(b)->box();
    CHECK(route.front().x >= ab.x);
    CHECK(route.front().x <= ab.right());
    CHECK(route.front().y >= ab.y);
    CHECK(route.front().y <= ab.bottom());
    CHECK(route.back().x >= bb.x);
    CHECK(route.back().x <= bb.right());
    CHECK(route.back().y >= bb.y);
    CHECK(route.back().y <= bb.bottom());
}
