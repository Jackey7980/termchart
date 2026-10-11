// test_graph_render.cpp —— 形状绘制与流程图端到端渲染
#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/graph/diagrams/flowchart.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/shape/shape.hpp"

namespace {

using namespace termchart;
using namespace termchart::graph;

}  // namespace

TEST_CASE("graph/shape: 矩形轮廓是四角加四条边，内部留空") {
    const std::vector<Point> outline = shape_outline(ShapeKind::Box, Rect{0, 0, 4, 3});
    // 四角 + 上下边各 2 + 左右边各 1 = 10
    CHECK(outline.size() == 10);
    CHECK(std::find(outline.begin(), outline.end(), Point{0, 0}) != outline.end());
    CHECK(std::find(outline.begin(), outline.end(), Point{3, 0}) != outline.end());
    CHECK(std::find(outline.begin(), outline.end(), Point{0, 2}) != outline.end());
    CHECK(std::find(outline.begin(), outline.end(), Point{3, 2}) != outline.end());
}

TEST_CASE("graph/shape: draw_shape 落格且不画内部") {
    canvas::Canvas canvas(6, 4);
    draw_shape(canvas, ShapeKind::Box, Rect{0, 0, 4, 3}, Style{});
    CHECK(canvas.at(0, 0).cp == U'\u250C');
    CHECK(canvas.at(3, 0).cp == U'\u2510');
    CHECK(canvas.at(0, 2).cp == U'\u2514');
    CHECK(canvas.at(3, 2).cp == U'\u2518');
    CHECK(canvas.at(1, 0).cp == U'\u2500');
    CHECK(canvas.at(0, 1).cp == U'\u2502');
    CHECK(canvas.at(1, 1).cp == U' ');  // 内部为空

    canvas::Canvas rounded(6, 4);
    draw_shape(rounded, ShapeKind::Rounded, Rect{0, 0, 4, 3}, Style{});
    CHECK(rounded.at(0, 0).cp == U'\u256D');

    canvas::Canvas diamond(6, 5);
    draw_shape(diamond, ShapeKind::Diamond, Rect{0, 0, 5, 5}, Style{});
    CHECK(diamond.at(2, 0).cp != U' ');  // 顶点
}

TEST_CASE("graph/diagrams: 流程图端到端绘制出边框、标签与箭头") {
    Graph graph;
    const NodeId a = graph.add_node("aaa").id();
    const NodeId b = graph.add_node("bbb").id();
    (void)graph.add_edge(a, b);

    canvas::Canvas canvas(40, 12);
    const Rect bounds = render_flowchart(graph, canvas, FlowchartOptions{});
    CHECK(bounds.w > 0.0);
    CHECK(bounds.h > 0.0);

    bool has_corner = false;
    bool has_arrow = false;
    bool has_label = false;
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            const char32_t cp = canvas.at(x, y).cp;
            has_corner = has_corner || cp == U'\u250C';
            has_arrow = has_arrow || cp == U'\u25B6';
            has_label = has_label || cp == U'a' || cp == U'b';
        }
    }
    CHECK(has_corner);
    CHECK(has_arrow);   // 目标侧箭头
    CHECK(has_label);
}
