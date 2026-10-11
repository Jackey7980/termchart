// test_diagrams_orgchart.cpp —— 组织架构图（层级树 + 树干 + 接点收口）
#include <doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/graph/diagrams/orgchart.hpp"
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

// 某一行上是否连续出现给定 ASCII 文本（确认节点标签没被连线涂掉）。
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

Graph make_org() {
    Graph graph;
    const NodeId ceo = graph.add_node("CEO").id();
    const NodeId cto = graph.add_node("CTO").id();
    const NodeId cfo = graph.add_node("CFO").id();
    const NodeId eng = graph.add_node("Eng").id();
    (void)graph.add_edge(ceo, cto);
    (void)graph.add_edge(ceo, cfo);
    (void)graph.add_edge(cto, eng);
    return graph;
}

}  // namespace

TEST_CASE("diagrams/orgchart: 层级树、树干列与接点收口") {
    Graph graph = make_org();
    canvas::Canvas canvas(60, 14);
    const Rect bounds = render_orgchart(graph, canvas, OrgChartOptions{});

    CHECK(bounds.w > 0.0);
    CHECK(bounds.h > 0.0);

    // 四个节点的标签都在（没有任何一个被连线涂掉）
    CHECK(row_contains(canvas, "CEO"));
    CHECK(row_contains(canvas, "CTO"));
    CHECK(row_contains(canvas, "CFO"));
    CHECK(row_contains(canvas, "Eng"));

    // 每条边都是轴对齐折线，且竖直段落在"子节点框左侧前一列"（= 树干列）
    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        REQUIRE(route.size() >= 2);
        for (std::size_t i = 1; i < route.size(); ++i) {
            const bool axis_aligned =
                route[i].x == route[i - 1].x || route[i].y == route[i - 1].y;
            CHECK(axis_aligned);
        }
        const Node* from = graph.find(edge.from());
        const Node* to = graph.find(edge.to());
        REQUIRE(from != nullptr);
        REQUIRE(to != nullptr);
        // 树干列的不变量（这三条各钉住一类观感 bug）：
        //   1) 不在父框边框列上 —— 否则父侧横线长度会是 0；
        //   2) 间距够宽时不贴着子框边框 —— 否则渲染成 `├│` 那样糊在一起；
        //   3) 两个接点在同一列 —— 这才是"树干"而不是两条独立折线。
        CHECK(route[1].x > from->box().right() - 1.0);
        if (to->box().x - 1.0 > from->box().right()) {
            CHECK(route[1].x < to->box().x - 1.0);
        }
        CHECK(route[1].x == doctest::Approx(route[2].x));
    }

    // 接点收口：每条边的两个接点格要么是 T 形接点字符，要么是"单子节点直线"的横线——
    // 但绝不能是裸的竖线（那说明横竖两段各画各的、折角是断的）。
    const auto is_junction = [](char32_t cp) {
        switch (cp) {
            case U'\u250C':  // ┌
            case U'\u2510':  // ┐
            case U'\u2514':  // └
            case U'\u2518':  // ┘
            case U'\u251C':  // ├
            case U'\u2524':  // ┤
            case U'\u252C':  // ┬
            case U'\u2534':  // ┴
            case U'\u253C':  // ┼
                return true;
            default:
                return false;
        }
    };
    std::size_t junction_count = 0;
    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        REQUIRE(route.size() >= 4);
        for (const Point& turn : {route[1], route[2]}) {
            const char32_t cp = canvas.at(static_cast<std::uint32_t>(turn.x),
                                          static_cast<std::uint32_t>(turn.y))
                                    .cp;
            CHECK(cp != U'\u2502');  // 不是裸竖线
            if (is_junction(cp)) {
                ++junction_count;
            } else {
                CHECK(cp == U'\u2500');  // 单子节点直线：保持横线
            }
        }
    }
    // 多子节点处分叉必然产生真正的接点字符（CEO 有 CTO/CFO 两个孩子）
    CHECK(junction_count >= 2);

    // 组织架构图里不应出现斜线段（全部走树干，斜线是路由退化的信号）
    CHECK_FALSE(has_char(canvas, U'\u2572'));  // ╲
    CHECK_FALSE(has_char(canvas, U'\u2571'));  // ╱

    // 组织架构图不画箭头（层级由树干体现）
    CHECK_FALSE(has_char(canvas, U'\u25B6'));  // ▶
    CHECK_FALSE(has_char(canvas, U'\u25BC'));  // ▼
}

TEST_CASE("diagrams/orgchart: 单节点只画框与标签，空图不崩") {
    Graph single;
    (void)single.add_node("Solo").id();
    canvas::Canvas canvas(20, 6);
    const Rect bounds = render_orgchart(single, canvas, OrgChartOptions{});
    CHECK(bounds.w > 0.0);
    CHECK(row_contains(canvas, "Solo"));
    CHECK(has_char(canvas, U'\u250C'));  // ┌ 左上角（默认矩形形状）

    Graph empty;
    canvas::Canvas empty_canvas(10, 4);
    const Rect empty_bounds = render_orgchart(empty, empty_canvas, OrgChartOptions{});
    CHECK(empty_bounds.w == doctest::Approx(0.0));
    CHECK(empty_bounds.h == doctest::Approx(0.0));
    CHECK_FALSE(has_char(empty_canvas, U'\u250C'));  // 什么都没画
}

TEST_CASE("diagrams/orgchart: 画布放不下时按裁剪丢弃，不折回也不越界写") {
    Graph graph = make_org();
    canvas::Canvas tiny(6, 3);  // 比布局结果小得多
    const Rect bounds = render_orgchart(graph, tiny, OrgChartOptions{});
    CHECK(bounds.w > 0.0);  // 布局结果仍在（画布小不影响布局）

    // 越界内容被裁剪：画布内不应出现明显的越界折回痕迹（内容按格写入即可）
    for (std::uint32_t y = 0; y < tiny.height(); ++y) {
        for (std::uint32_t x = 0; x < tiny.width(); ++x) {
            const char32_t cp = tiny.at(x, y).cp;
            CHECK(cp != U'\0');
        }
    }
}
