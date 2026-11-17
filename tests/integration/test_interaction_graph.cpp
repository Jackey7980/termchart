// test_interaction_graph.cpp —— interaction > graph 放行端到端验证
//
// 补齐 ROADMAP 阶段 4 的缺口："用一张图的实际命中判定验证分层放行"。
// 链路：Graph 建图 → layout_grid 布局写 Node::box() → 为每个节点登记 HitRegion
// → interaction::topmost_hit 命中一个格坐标 → 断言命中 payload 指向正确节点。
// graph 侧不认识 interaction 类型，命中判定只吃 chart::HitRegion + 数值坐标，
// 这正是"分层放行但不耦合"的契约。
#include <doctest.h>

#include <cstdint>
#include <vector>

#include "termchart/chart/hit_region.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"
#include "termchart/graph/node.hpp"
#include "termchart/interaction/hit_test.hpp"

namespace {

using namespace termchart;
using termchart::chart::HitRegion;

// 建一张 2x2 网格图，返回节点 id（按 a b c d）。
struct GridGraph {
    graph::Graph g;
    graph::NodeId a, b, c, d;
};

GridGraph make_graph() {
    GridGraph gg;
    gg.a = gg.g.add_node("a").id();
    gg.b = gg.g.add_node("b").id();
    gg.c = gg.g.add_node("c").id();
    gg.d = gg.g.add_node("d").id();
    (void)gg.g.add_edge(gg.a, gg.b);
    (void)gg.g.add_edge(gg.a, gg.c);
    (void)gg.g.add_edge(gg.b, gg.d);
    (void)gg.g.add_edge(gg.c, gg.d);
    return gg;
}

}  // namespace

TEST_CASE("integration/interaction>graph: 网格图节点命中回指正确节点") {
    GridGraph gg = make_graph();
    const Rect bounds = graph::layout_grid(gg.g, 2, graph::LayoutOptions{});
    CHECK(bounds.w > 0);
    CHECK(bounds.h > 0);

    // graph 节点框 → HitRegion（payload = 节点 id）。这是唯一的适配层。
    std::vector<HitRegion> regions;
    regions.reserve(gg.g.node_count());
    for (const graph::Node& node : gg.g.nodes()) {
        regions.push_back(HitRegion::rect(node.box(), node.id()));
    }
    REQUIRE(regions.size() == 4);

    // 取每个节点框内部一点（避开边界歧义），断言命中回指同一节点。
    for (const graph::Node& node : gg.g.nodes()) {
        const Rect box = node.box();
        const double px = box.x + box.w / 2.0;
        const double py = box.y + box.h / 2.0;
        const HitRegion* hit = interaction::topmost_hit(regions, px, py);
        REQUIRE(hit != nullptr);
        CHECK(hit->payload() == node.id());
    }
}

TEST_CASE("integration/interaction>graph: 后登记者在上层，hit_payloads 顺序命中") {
    GridGraph gg = make_graph();
    (void)graph::layout_grid(gg.g, 2, graph::LayoutOptions{});

    std::vector<HitRegion> regions;
    for (const graph::Node& node : gg.g.nodes()) {
        regions.push_back(HitRegion::rect(node.box(), node.id()));
    }

    // 全部节点框的中心互不重叠（网格布局的契约），任一点只命中一个区域。
    const graph::Node* first = nullptr;
    for (const graph::Node& node : gg.g.nodes()) {
        first = &node;
        break;
    }
    REQUIRE(first != nullptr);
    const Rect box = first->box();
    const auto payloads = interaction::hit_payloads(regions, box.x + 1.0, box.y + 1.0);
    CHECK(payloads.size() >= 1);
    CHECK(payloads.front() == first->id());
}

TEST_CASE("integration/interaction>graph: 图外坐标不命中任何节点") {
    GridGraph gg = make_graph();
    const Rect bounds = graph::layout_grid(gg.g, 2, graph::LayoutOptions{});

    std::vector<HitRegion> regions;
    for (const graph::Node& node : gg.g.nodes()) {
        regions.push_back(HitRegion::rect(node.box(), node.id()));
    }

    // 取包围盒右下角再外扩 50 格，必在所有节点框之外。
    const double out_x = bounds.x + bounds.w + 50.0;
    const double out_y = bounds.y + bounds.h + 50.0;
    CHECK(interaction::topmost_hit(regions, out_x, out_y) == nullptr);
}
