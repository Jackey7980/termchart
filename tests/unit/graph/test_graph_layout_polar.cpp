// test_graph_layout_polar.cpp —— 径向布局 radial 与圆周布局 circular 的几何不变量
//
// 这里断言的是“几何关系”而非具体坐标：层半径单调、同层等距、兄弟角间隔相等、
// 圆周等距、相邻不重叠、确定性、退化输入不崩。具体坐标由取的环距/取整决定，
// 断言它们会把无关实现细节固化成契约，因此只查关系与容差。
#include <doctest.h>

#include <cmath>
#include <cstddef>
#include <vector>

#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/circular.hpp"
#include "termchart/graph/layout/radial.hpp"

namespace {

using namespace termchart;
using namespace termchart::graph;

constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kPi = 3.14159265358979323846;

// 在“宽高比口径”下量两点的几何距离：x 正常、y 乘 aspect，还原到方格空间。
// 布局把 y 压缩了 1/aspect，只有按同口径还原，半径/角度才等于几何量。
double polar_distance(Point a, Point b, double aspect) {
    const double dx = a.x - b.x;
    const double dy = (a.y - b.y) * aspect;
    return std::hypot(dx, dy);
}

// 以 center 为极点量 p 的极角（同样按宽高比口径还原 y）。
double polar_angle(Point center, Point p, double aspect) {
    return std::atan2((p.y - center.y) * aspect, p.x - center.x);
}

// 归一到 (-π, π]。
double wrap_pi(double a) {
    while (a > kPi) {
        a -= kTwoPi;
    }
    while (a <= -kPi) {
        a += kTwoPi;
    }
    return a;
}

Point center_of(const Graph& graph, NodeId id) { return graph.find(id)->box().center(); }

// 每个节点框都落在并集内（含边界）。
void check_all_within_bounds(const Graph& graph, const Rect& bounds) {
    for (const Node& n : graph.nodes()) {
        CHECK(n.box().x >= bounds.x - 1e-9);
        CHECK(n.box().y >= bounds.y - 1e-9);
        CHECK(n.box().right() <= bounds.right() + 1e-9);
        CHECK(n.box().bottom() <= bounds.bottom() + 1e-9);
    }
}

// 每个节点框不小于 LayoutOptions 给的最小尺寸。
void check_min_size(const Graph& graph, const LayoutOptions& lo) {
    for (const Node& n : graph.nodes()) {
        CHECK(n.box().w >= lo.min_width - 1e-9);
        CHECK(n.box().h >= lo.min_height - 1e-9);
    }
}

// 造一棵 5 节点树：root → {a → a1, b → b1}。两个用例共用（确定性要两份等价的图）。
void build_sample_tree(Graph& graph, NodeId& root, NodeId& a, NodeId& b, NodeId& a1,
                       NodeId& b1) {
    root = graph.add_node("root").id();
    a = graph.add_node("a").id();
    b = graph.add_node("b").id();
    a1 = graph.add_node("a1").id();
    b1 = graph.add_node("b1").id();
    (void)graph.add_edge(root, a);
    (void)graph.add_edge(root, b);
    (void)graph.add_edge(a, a1);
    (void)graph.add_edge(b, b1);
}

}  // namespace

TEST_CASE("graph/layout/radial: 空图返回零矩形") {
    Graph graph;
    CHECK(layout_radial(graph, RadialOptions{}).w == doctest::Approx(0.0));
}

TEST_CASE("graph/layout/radial: 层号越大离根越远，同层等距") {
    Graph graph;
    NodeId root = 0;
    NodeId a = 0;
    NodeId b = 0;
    NodeId a1 = 0;
    NodeId b1 = 0;
    build_sample_tree(graph, root, a, b, a1, b1);

    RadialOptions opts;
    // 大环距 → 取整落格的 ±1 格误差相对更小，几何断言更干净。
    opts.ring_step = 40.0;
    const Rect bounds = layout_radial(graph, opts);

    CHECK(graph.node_count() == std::size_t{5});
    check_all_within_bounds(graph, bounds);
    check_min_size(graph, opts.layout);

    const Point c = center_of(graph, root);
    const double d_root = polar_distance(c, center_of(graph, root), opts.aspect);
    const double d_a = polar_distance(c, center_of(graph, a), opts.aspect);
    const double d_b = polar_distance(c, center_of(graph, b), opts.aspect);
    const double d_a1 = polar_distance(c, center_of(graph, a1), opts.aspect);
    const double d_b1 = polar_distance(c, center_of(graph, b1), opts.aspect);

    // 根在圆心，第 1 层半径 ≈ 40，第 2 层 ≈ 80；取整误差 ≤ 约 2 格，容差 3 格足够。
    CHECK(d_root <= 1.0);
    CHECK(std::abs(d_a - 40.0) <= 3.0);
    CHECK(std::abs(d_b - 40.0) <= 3.0);
    CHECK(std::abs(d_a1 - 80.0) <= 3.0);
    CHECK(std::abs(d_b1 - 80.0) <= 3.0);

    // 同层等距（浮点用 Approx；epsilon 覆盖取整落格误差）。
    CHECK(d_a == doctest::Approx(d_b).epsilon(0.08));
    CHECK(d_a1 == doctest::Approx(d_b1).epsilon(0.05));

    // 子节点比父节点更远。
    CHECK(d_a > d_root);
    CHECK(d_b > d_root);
    CHECK(d_a1 > d_a);
    CHECK(d_b1 > d_b);
}

TEST_CASE("graph/layout/radial: 同父兄弟角度间隔相等") {
    Graph graph;
    const NodeId root = graph.add_node("r").id();
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    const NodeId c = graph.add_node("c").id();
    (void)graph.add_edge(root, a);
    (void)graph.add_edge(root, b);
    (void)graph.add_edge(root, c);

    RadialOptions opts;
    opts.ring_step = 40.0;
    (void)layout_radial(graph, opts);

    const Point center = center_of(graph, root);
    const double ta = polar_angle(center, center_of(graph, a), opts.aspect);
    const double tb = polar_angle(center, center_of(graph, b), opts.aspect);
    const double tc = polar_angle(center, center_of(graph, c), opts.aspect);

    // 3 个孩子均分整圆 → 相邻间隔各为 2π/3。
    CHECK(wrap_pi(tb - ta) == doctest::Approx(kTwoPi / 3.0).epsilon(0.08));
    CHECK(wrap_pi(tc - tb) == doctest::Approx(kTwoPi / 3.0).epsilon(0.08));
    CHECK(wrap_pi(ta - tc) == doctest::Approx(kTwoPi / 3.0).epsilon(0.08));
}

TEST_CASE("graph/layout/radial: 确定性（同输入两次逐字段相等）") {
    Graph first;
    NodeId r1 = 0;
    NodeId a1 = 0;
    NodeId b1 = 0;
    NodeId c1 = 0;
    NodeId d1 = 0;
    build_sample_tree(first, r1, a1, b1, c1, d1);

    Graph second;
    NodeId r2 = 0;
    NodeId a2 = 0;
    NodeId b2 = 0;
    NodeId c2 = 0;
    NodeId d2 = 0;
    build_sample_tree(second, r2, a2, b2, c2, d2);

    RadialOptions opts;
    opts.ring_step = 20.0;
    opts.min_angle = 0.1;
    (void)layout_radial(first, opts);
    (void)layout_radial(second, opts);

    REQUIRE(first.nodes().size() == second.nodes().size());
    for (std::size_t i = 0; i < first.nodes().size(); ++i) {
        CHECK(first.nodes()[i].box() == second.nodes()[i].box());
    }
}

TEST_CASE("graph/layout/radial: 有环图 A→B→A 不崩，根取第一个节点") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, a);

    const Rect bounds = layout_radial(graph, RadialOptions{});
    CHECK(bounds.w > 0.0);
    check_all_within_bounds(graph, bounds);

    // 全有入边 → 根 = 第一个登记的节点 a，落在圆心；b 在第 1 层。
    const Point c = center_of(graph, a);
    const double d_a = polar_distance(c, center_of(graph, a), 2.0);
    const double d_b = polar_distance(c, center_of(graph, b), 2.0);
    CHECK(d_a <= 1.0);
    CHECK(d_b > 1.0);
    CHECK_FALSE(graph.find(a)->box() == graph.find(b)->box());
}

TEST_CASE("graph/layout/circular: 所有节点到圆心等距") {
    Graph graph;
    std::vector<NodeId> ids;
    for (int i = 0; i < 6; ++i) {
        ids.push_back(graph.add_node("n" + std::to_string(i)).id());
    }

    CircularOptions opts;
    const Rect bounds = layout_circular(graph, opts);
    CHECK(graph.node_count() == std::size_t{6});
    check_all_within_bounds(graph, bounds);
    check_min_size(graph, opts.layout);

    // 均匀分布点的中心 = 圆心（cos/sin 一周期求和为 0），不必知道实现内部坐标。
    Point center{};
    for (const NodeId id : ids) {
        const Point p = center_of(graph, id);
        center.x += p.x;
        center.y += p.y;
    }
    center.x /= static_cast<double>(ids.size());
    center.y /= static_cast<double>(ids.size());

    std::vector<double> dist;
    dist.reserve(ids.size());
    for (const NodeId id : ids) {
        dist.push_back(polar_distance(center, center_of(graph, id), opts.aspect));
    }

    // 等距：epsilon 覆盖取整落格误差。
    for (const double d : dist) {
        CHECK(d == doctest::Approx(dist.front()).epsilon(0.15));
        CHECK(d > 1.0);
    }
}

TEST_CASE("graph/layout/circular: 相邻节点框不重叠") {
    Graph graph;
    for (int i = 0; i < 6; ++i) {
        graph.add_node("node" + std::to_string(i));
    }

    const Rect bounds = layout_circular(graph, CircularOptions{});
    check_all_within_bounds(graph, bounds);

    const std::size_t n = graph.nodes().size();
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1) % n;
        // 半开区间口径：贴边不算相交。
        CHECK_FALSE(graph.nodes()[i].box().intersects(graph.nodes()[j].box()));
    }
}

TEST_CASE("graph/layout/circular: n = 1 / 2 / 3 不崩、不除零、坐标有限") {
    for (const std::size_t n : {std::size_t{1}, std::size_t{2}, std::size_t{3}}) {
        Graph graph;
        for (std::size_t i = 0; i < n; ++i) {
            graph.add_node("x");
        }

        const Rect bounds = layout_circular(graph, CircularOptions{});
        CHECK(bounds.w > 0.0);
        CHECK(std::isfinite(bounds.w));
        CHECK(std::isfinite(bounds.h));
        check_all_within_bounds(graph, bounds);
        check_min_size(graph, CircularOptions{}.layout);

        for (const Node& node : graph.nodes()) {
            CHECK(std::isfinite(node.box().x));
            CHECK(std::isfinite(node.box().y));
        }

        if (n >= 2) {
            for (std::size_t i = 0; i < n; ++i) {
                const std::size_t j = (i + 1) % n;
                CHECK_FALSE(graph.nodes()[i].box().intersects(graph.nodes()[j].box()));
            }
        }
    }
}

TEST_CASE("graph/layout/circular: 空图返回零矩形") {
    Graph graph;
    CHECK(layout_circular(graph, CircularOptions{}).w == doctest::Approx(0.0));
}
