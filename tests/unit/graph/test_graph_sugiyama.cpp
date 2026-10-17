// test_graph_sugiyama.cpp —— Sugiyama 四步布局：分层 → 层内降交叉 → 坐标分配
//
// 这里锁的是"降交叉真的生效且收敛"：交叉数必须下降，且迭代轮数不该影响结果
// （曾经 up-sweep 用上一层旧位置 + 用节点登记索引做 tie-break，偶数轮会把
// down-sweep 的成果推翻回登记顺序，交叉数原地不动）。
#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "termchart/graph/crossing.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"
#include "termchart/graph/layout/sugiyama.hpp"
#include "termchart/graph/route/route.hpp"
#include "termchart/graph/wiregrid.hpp"

namespace {

using namespace termchart;
using namespace termchart::graph;

// 层内顺序：同层共享 x，按 x 分组、组内按 y（y 才是布局排出来的次序）。
std::string layer_labels(const Graph& graph) {
    std::map<double, std::vector<std::pair<double, std::string>>> by_x;
    for (const Node& node : graph.nodes()) {
        by_x[node.box().x].emplace_back(node.box().y, node.label());
    }
    std::string out;
    for (auto& entry : by_x) {
        std::sort(entry.second.begin(), entry.second.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& placed : entry.second) {
            out += placed.second;
        }
        out += "|";
    }
    return out;
}

std::size_t center_crossings(const Graph& graph) {
    std::vector<std::pair<Point, Point>> segments;
    segments.reserve(graph.edge_count());
    for (const Edge& edge : graph.edges()) {
        const Node* from = graph.find(edge.from());
        const Node* to = graph.find(edge.to());
        if (from == nullptr || to == nullptr) {
            continue;
        }
        segments.emplace_back(from->box().center(), to->box().center());
    }
    return count_crossings(segments);
}

// 演示图：L0: A B C、L1: D E F、L2: G H、L3: I J，边刻意逆序相连必然交叉。
Graph make_crossing_graph() {
    Graph graph;
    const NodeId a = graph.add_node("A").id();
    const NodeId b = graph.add_node("B").id();
    const NodeId c = graph.add_node("C").id();
    const NodeId d = graph.add_node("D").id();
    const NodeId e = graph.add_node("E").id();
    const NodeId f = graph.add_node("F").id();
    const NodeId g = graph.add_node("G").id();
    const NodeId h = graph.add_node("H").id();
    const NodeId i = graph.add_node("I").id();
    const NodeId j = graph.add_node("J").id();
    (void)graph.add_edge(a, f);
    (void)graph.add_edge(b, f);
    (void)graph.add_edge(b, e);
    (void)graph.add_edge(c, d);
    (void)graph.add_edge(f, g);
    (void)graph.add_edge(e, g);
    (void)graph.add_edge(d, h);
    (void)graph.add_edge(g, i);
    (void)graph.add_edge(h, j);
    (void)graph.add_edge(c, f);  // 桥接：把 {a,b,e,f,g,i} 与 {c,d,h,j} 两条链合成一张连通图
    return graph;
}

LayoutOptions test_options() {
    LayoutOptions opts;
    opts.min_width = 8.0;
    opts.min_height = 4.0;
    opts.h_gap = 6.0;
    opts.v_gap = 2.0;
    opts.margin = 1.0;
    return opts;
}

// 确定性 LCG：测试图必须每次一样，不用 <random>（分布实现之间会有差异）。
struct Lcg {
    std::uint32_t state = 0x2545F491u;
    std::uint32_t next() {
        state = state * 1664525u + 1013904223u;
        return (state >> 16) & 0x7fffu;
    }
};

// 7 层 × 5 节点的中等密集图：层间每个节点随机连 1~2 条，另加几条跨两层长边。
// 给"降交叉 + 长度 tie-break + dummy 参与排序"一个有一定规模的回归靶子。
Graph make_dense_regression_graph() {
    Graph graph;
    Lcg rng;
    std::vector<std::vector<NodeId>> layers(7);
    for (std::size_t l = 0; l < 7; ++l) {
        for (std::size_t k = 0; k < 5; ++k) {
            layers[l].push_back(
                graph.add_node("n" + std::to_string(l) + "_" + std::to_string(k)).id());
        }
    }
    for (std::size_t l = 0; l + 1 < 7; ++l) {
        for (std::size_t a = 0; a < 5; ++a) {
            const std::size_t count = 1 + rng.next() % 2;
            for (std::size_t c = 0; c < count; ++c) {
                (void)graph.add_edge(layers[l][a], layers[l + 1][rng.next() % 5]);
            }
        }
    }
    for (std::size_t l = 0; l + 2 < 7; l += 2) {  // 跨两层：必须插 dummy
        (void)graph.add_edge(layers[l][rng.next() % 5], layers[l + 2][rng.next() % 5]);
    }
    return graph;
}

double route_length(const Graph& graph) {
    double sum = 0.0;
    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& path = edge.route();
        for (std::size_t i = 1; i < path.size(); ++i) {
            sum += std::abs(path[i].x - path[i - 1].x) + std::abs(path[i].y - path[i - 1].y);
        }
    }
    return sum;
}

}  // namespace

TEST_CASE("graph::layout_sugiyama: 分层为四层") {
    Graph graph = make_crossing_graph();
    const Rect bounds = layout_sugiyama(graph, test_options(), 4);

    CHECK(bounds.w > 0.0);
    CHECK(bounds.h > 0.0);
    // 列 = rank：四层各占一列，x 严格递增。
    std::vector<double> columns;
    for (const Node& node : graph.nodes()) {
        if (std::find(columns.begin(), columns.end(), node.box().x) == columns.end()) {
            columns.push_back(node.box().x);
        }
    }
    std::sort(columns.begin(), columns.end());
    REQUIRE(columns.size() == 4);
    CHECK(columns[0] < columns[1]);
    CHECK(columns[1] < columns[2]);
    CHECK(columns[2] < columns[3]);
}

TEST_CASE("graph::layout_sugiyama: 降交叉把交叉数降到 0") {
    const LayoutOptions opts = test_options();

    Graph base = make_crossing_graph();
    (void)layout_layered(base, opts);
    const std::size_t before = center_crossings(base);

    Graph graph = make_crossing_graph();
    (void)layout_sugiyama(graph, opts, 4);
    const std::size_t after = center_crossings(graph);

    CHECK(before > 0);   // 基线确实有交叉，否则这条用例没有意义
    CHECK(after == 0);   // 降交叉后中心连线两两不交叉
    CHECK(after < before);
    // 层内次序被重排：L1 由登记顺序 D E F 变成 F E D（桥接边 c→f 引入后 c/e 各前移一位）。
    CHECK(layer_labels(graph) == "BAC|EFD|GH|IJ|");
}

TEST_CASE("graph::layout_sugiyama: 迭代轮数不影响收敛结果") {
    // 曾出现偶数轮退化回登记顺序（up-sweep 用了同轮内已改层的旧位置），
    // 这里把 1..8 轮的结果全部钉成同一个。
    const std::string expected = "BAC|EFD|GH|IJ|";
    for (std::uint32_t iterations = 1; iterations <= 8; ++iterations) {
        Graph graph = make_crossing_graph();
        (void)layout_sugiyama(graph, test_options(), iterations);
        CHECK(layer_labels(graph) == expected);
        CHECK(center_crossings(graph) == 0);
    }
}

TEST_CASE("graph::layout_sugiyama: 单层交叉也被解开") {
    // P→S、Q→R 在 L0→L1 必然交叉；用共享子节点 X 把两条边连成一张连通图
    // （否则两对节点是互不相连的分量，分量分离会把它们错开成左右两块、反而不交叉）。
    Graph graph;
    const NodeId p = graph.add_node("P").id();
    const NodeId q = graph.add_node("Q").id();
    const NodeId r = graph.add_node("R").id();
    const NodeId s = graph.add_node("S").id();
    const NodeId x = graph.add_node("X").id();
    (void)graph.add_edge(p, s);  // P→S、Q→R 必然交叉
    (void)graph.add_edge(q, r);
    (void)graph.add_edge(s, x);
    (void)graph.add_edge(r, x);

    (void)layout_sugiyama(graph, test_options(), 4);
    CHECK(layer_labels(graph) == "PQ|SR|X|");
    CHECK(center_crossings(graph) == 0);
}

TEST_CASE("graph::layout_sugiyama: 空图返回空 Rect") {
    Graph graph;
    const Rect bounds = layout_sugiyama(graph, test_options(), 4);
    CHECK(bounds.w == doctest::Approx(0.0));
    CHECK(bounds.h == doctest::Approx(0.0));
}

// 跨两层的长边示范图：A(层 0) → C(层 2)，中间层只有 B。A、B、C 都在同一行，
// 所以"直连行"正好压在 B 的框上——没有 dummy 槽位时，通用 Z 形必然被 B 否决。
Graph make_long_edge_graph() {
    Graph graph;
    const NodeId a = graph.add_node("A").id();
    const NodeId b = graph.add_node("B").id();
    const NodeId c = graph.add_node("C").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, c);
    (void)graph.add_edge(a, c);  // 长边：rank 差 2
    return graph;
}

// 折线逐格采样（轴对齐段按最大轴向步数展开）。
std::vector<Point> sample_cells(const std::vector<Point>& route) {
    std::vector<Point> cells;
    for (std::size_t i = 1; i < route.size(); ++i) {
        const double span = std::max(std::fabs(route[i].x - route[i - 1].x),
                                     std::fabs(route[i].y - route[i - 1].y));
        const auto steps = static_cast<int>(std::ceil(span));
        for (int s = 0; s <= steps; ++s) {
            const double t = static_cast<double>(s) / static_cast<double>(steps);
            cells.push_back(Point{std::round(route[i - 1].x + (route[i].x - route[i - 1].x) * t),
                                  std::round(route[i - 1].y + (route[i].y - route[i - 1].y) * t)});
        }
    }
    return cells;
}

TEST_CASE("graph::layout_sugiyama: 跨层长边走 dummy 槽位且不穿中间层框") {
    Graph graph = make_long_edge_graph();
    const LayoutOptions opts = test_options();
    (void)layout_sugiyama(graph, opts, 4);

    const NodeId a = graph.nodes()[0].id();
    const NodeId b = graph.nodes()[1].id();
    const NodeId c = graph.nodes()[2].id();
    const Edge* long_edge = graph.find_edge(a, c);
    REQUIRE(long_edge != nullptr);

    // 长边骨架：横 → 竖 → 横穿槽位行 → 竖 → 横（共线时会简化到 4 点）。
    const std::vector<Point>& route = long_edge->route();
    REQUIRE(route.size() >= 4);
    for (std::size_t i = 1; i < route.size(); ++i) {
        CHECK_MESSAGE((route[i].x == route[i - 1].x || route[i].y == route[i - 1].y),
                      "seg ", i, " is not axis-aligned");
    }

    // 中间格不得进入挡路的中间层节点 B 的框（端点 A/C 的框不算）。
    const Rect box_b = graph.find(b)->box();
    const std::vector<Point> cells = sample_cells(route);
    REQUIRE(cells.size() > 2);
    std::size_t checked = 0;
    for (std::size_t i = 1; i + 1 < cells.size(); ++i) {
        const bool inside = cells[i].x >= box_b.x && cells[i].x < box_b.right() &&
                            cells[i].y >= box_b.y && cells[i].y < box_b.bottom();
        CHECK_MESSAGE(!inside, "cell (", cells[i].x, ",", cells[i].y, ") enters middle box");
        ++checked;
    }
    CHECK(checked > 0);

    // 约定：布局之后 route 非空 ⟺ 长边骨架；相邻层边由路由函数负责。
    const Edge* short_edge = graph.find_edge(a, b);
    REQUIRE(short_edge != nullptr);
    CHECK(short_edge->route().empty());
}

TEST_CASE("graph::route_edges: keep_existing_routes 保留布局的长边骨架") {
    Graph graph = make_long_edge_graph();
    const Rect bounds = layout_sugiyama(graph, test_options(), 4);

    const NodeId a = graph.nodes()[0].id();
    const NodeId c = graph.nodes()[2].id();
    const Edge* long_edge = graph.find_edge(a, c);
    REQUIRE(long_edge != nullptr);
    const std::vector<Point> skeleton = long_edge->route();
    REQUIRE(skeleton.size() >= 4);

    const auto w = static_cast<std::uint32_t>(std::ceil(bounds.right())) + 2u;
    const auto h = static_cast<std::uint32_t>(std::ceil(bounds.bottom())) + 2u;

    {  // 默认 false：长边被路由重算（形状不锁，只要求仍是轴对齐折线）
        Graph copy = graph;
        WireGrid grid(w, h);
        RouteOptions opts;
        (void)route_edges(copy, RouteStyle::Orthogonal, opts, &grid);
        const std::vector<Point>& rerouted = copy.find_edge(a, c)->route();
        REQUIRE(rerouted.size() >= 2);
        for (std::size_t i = 1; i < rerouted.size(); ++i) {
            CHECK((rerouted[i].x == rerouted[i - 1].x || rerouted[i].y == rerouted[i - 1].y));
        }
    }
    {  // true：骨架原样保留
        Graph copy = graph;
        WireGrid grid(w, h);
        RouteOptions opts;
        opts.keep_existing_routes = true;
        (void)route_edges(copy, RouteStyle::Orthogonal, opts, &grid);
        const std::vector<Point>& kept = copy.find_edge(a, c)->route();
        REQUIRE(kept.size() == skeleton.size());
        for (std::size_t i = 0; i < kept.size(); ++i) {
            CHECK(kept[i].x == skeleton[i].x);
            CHECK(kept[i].y == skeleton[i].y);
        }
    }
}

TEST_CASE("graph::layout_sugiyama: 中等密集图的交叉数与走线长度回归上界") {
    Graph graph = make_dense_regression_graph();
    (void)layout_sugiyama(graph, test_options(), 8);

    const std::size_t crossings = center_crossings(graph);
    const double length = route_length(graph);
    std::printf("dense regression: crossings=%zu length=%.0f\n", crossings, length);

    // 钉的是"不会倒退"，不是精确值（精确值随算法演进会变，留 ~40% 余量）。
    // 当前实测：crossings=17、length=69（network simplex 分层：rank 更紧 ⇒ dummy
    // 更少 ⇒ 长边骨架从 720 格缩到 69 格）。
    CHECK(crossings <= 25);
    CHECK(length <= 200.0);
}

// ── 破环（回边反转）──────────────────────────────────────────────────
// X → A → B → A 是能触发旧塌陷的最小图：X 是唯一源，Kahn 推到 A 就卡住（A 还在等
// B，B 又在等 A），兜底把 B 挂到"没有已处理前驱" ⇒ rank 0，于是 B 落回 X 那一列。
// 先反转回边 B→A 成 A→B 之后，整图是一条链，三层各一个节点。
Graph make_cycle_graph() {
    Graph graph;
    const NodeId x = graph.add_node("X").id();
    const NodeId a = graph.add_node("A").id();
    const NodeId b = graph.add_node("B").id();
    (void)graph.add_edge(x, a);
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, a);  // 回边：A → B → A 成环
    return graph;
}

TEST_CASE("graph::layout_sugiyama: 含环图先反转回边，分层不塌陷") {
    Graph graph = make_cycle_graph();
    (void)layout_sugiyama(graph, test_options(), 4);
    CHECK(layer_labels(graph) == "X|A|B|");
}

TEST_CASE("graph::layout_layered: 走同一份去环（before 基线不能被污染）") {
    // before/after 的差值要能归因到降交叉，就要求两次布局面对同一张 DAG。
    Graph graph = make_cycle_graph();
    (void)layout_layered(graph, test_options());
    CHECK(layer_labels(graph) == "X|A|B|");
}

TEST_CASE("graph::layout_sugiyama: 纯环被打开成链") {
    Graph graph;
    const NodeId a = graph.add_node("A").id();
    const NodeId b = graph.add_node("B").id();
    const NodeId c = graph.add_node("C").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, c);
    (void)graph.add_edge(c, a);

    (void)layout_sugiyama(graph, test_options(), 4);
    CHECK(layer_labels(graph) == "A|B|C|");
}

TEST_CASE("graph::layout_sugiyama: 自环被丢弃，不拖住 rank") {
    Graph graph;
    const NodeId a = graph.add_node("A").id();
    const NodeId b = graph.add_node("B").id();
    (void)graph.add_edge(a, a);  // 自环：留着会让 rank 永远抬不起来
    (void)graph.add_edge(a, b);

    (void)layout_sugiyama(graph, test_options(), 4);
    CHECK(layer_labels(graph) == "A|B|");
}

TEST_CASE("graph::layout_sugiyama: 含环图的长边骨架仍然轴对齐且不穿框") {
    // 含环 + 跨层长边：回边被反转后 A→C 变成"从右往左"的逆行长边，dummy 必须按
    // from → to 的走向走（曾经按 rank 升序遍历，折线会先跳到最左再折回来）。
    Graph graph;
    const NodeId a = graph.add_node("A").id();
    const NodeId b = graph.add_node("B").id();
    const NodeId c = graph.add_node("C").id();
    const NodeId d = graph.add_node("D").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, c);
    (void)graph.add_edge(c, d);
    (void)graph.add_edge(d, b);  // 回边：B→C→D→B 成环
    (void)graph.add_edge(a, d);  // 长边（跨多层）

    (void)layout_sugiyama(graph, test_options(), 4);
    const Edge* long_edge = graph.find_edge(a, d);
    REQUIRE(long_edge != nullptr);
    if (long_edge->route().empty()) {
        return;  // 反转后 A、D 恰好相邻层：骨架不参与，此用例无事可验
    }
    const std::vector<Point>& route = long_edge->route();
    REQUIRE(route.size() >= 2);
    for (std::size_t i = 1; i < route.size(); ++i) {
        CHECK_MESSAGE((route[i].x == route[i - 1].x || route[i].y == route[i - 1].y),
                      "seg ", i, " is not axis-aligned");
    }
    // 路径端点必须贴在真正的两端：front 是 A 那一侧、back 是 D 那一侧。
    const Rect box_a = graph.find(a)->box();
    const Rect box_d = graph.find(d)->box();
    CHECK_MESSAGE(std::fabs(route.front().y - (box_a.y + box_a.h / 2.0)) < 1.5,
                  "route should start at A, got ", route.front().y);
    CHECK_MESSAGE(std::fabs(route.back().y - (box_d.y + box_d.h / 2.0)) < 1.5,
                  "route should end at D, got ", route.back().y);
}

TEST_CASE("graph::layout_sugiyama: 两个断开分量被 cluster_gap 错开（X 区间不重叠）") {
    // 两条互不相连的链 A→B→C 与 D→E→F。无分量分离时，两者会被 NS 排进同一批列、
    // 同层节点互相穿插而视觉糊在一起；分离后应沿 X 错开成左右两块、区间不重叠。
    Graph graph;
    const NodeId a = graph.add_node("A").id();
    const NodeId b = graph.add_node("B").id();
    const NodeId c = graph.add_node("C").id();
    const NodeId d = graph.add_node("D").id();
    const NodeId e = graph.add_node("E").id();
    const NodeId f = graph.add_node("F").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, c);
    (void)graph.add_edge(d, e);
    (void)graph.add_edge(e, f);

    (void)layout_sugiyama(graph, test_options(), 4);

    auto x_range = [&](char first, char last) {
        double lo = 1e9, hi = -1e9;
        for (const Node& node : graph.nodes()) {
            const std::string lbl = node.label();
            if (!lbl.empty() && lbl[0] >= first && lbl[0] <= last) {
                lo = std::min(lo, node.box().x);
                hi = std::max(hi, node.box().right());
            }
        }
        return std::make_pair(lo, hi);
    };
    const auto r1 = x_range('A', 'C');
    const auto r2 = x_range('D', 'F');
    const bool separated = (r1.second < r2.first) || (r2.second < r1.first);
    CHECK(separated);
}
