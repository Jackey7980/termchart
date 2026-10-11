// flowchart.cpp —— 流程图渲染实现
//
// 绘制顺序刻意分三段：先连线，再节点（边框 + 标签），最后箭头。
// 理由：箭头的落点就在节点边框上，若先画箭头会被边框覆盖；反之若最后画连线，
// 线头会盖住边框与标签。三段顺序同时保证"框内干净、箭头可见"。
#include "termchart/graph/diagrams/flowchart.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/graph/arrow.hpp"
#include "termchart/graph/port.hpp"
#include "termchart/graph/wiregrid.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::graph {

namespace {

// 轴对齐线段用直线字符，斜线段按走向选对角线字符。
char32_t segment_char(Point a, Point b) noexcept {
    if (a.y == b.y) {
        return U'\u2500';  // 横线
    }
    if (a.x == b.x) {
        return U'\u2502';  // 竖线
    }
    // 屏幕坐标 y 向下：x 与 y 同向增长为右下斜线，反向为右上斜线。
    const bool down = (b.y > a.y) == (b.x > a.x);
    return down ? U'\u2572' : U'\u2571';
}

void put_arrow(canvas::Canvas& canvas, Point from, Point tip, const Style& style) {
    canvas.put(tip, arrow_head_char(side_towards(from, tip)), style);
}

}  // namespace

Rect render_flowchart(Graph& graph, canvas::Canvas& canvas,
                      const FlowchartOptions& opts) {
    const Rect bounds = layout_layered(graph, opts.layout);

    WireGrid grid(canvas.width(), canvas.height());
    (void)route_edges(graph, opts.route_style, opts.route, &grid);

    // 第一段：连线（不含箭头）。
    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        for (std::size_t i = 1; i < route.size(); ++i) {
            canvas.draw_line(route[i - 1], route[i],
                             segment_char(route[i - 1], route[i]), opts.edge_style);
        }
    }

    // 第二段：节点（形状边框 + 居中标签）。
    for (const Node& node : graph.nodes()) {
        draw_shape(canvas, opts.shape, node.box(), opts.node_style);
        if (node.label().empty()) {
            continue;
        }
        const int label_width = util::display_width(node.label());
        const double x = node.box().x +
                         std::max(0.0, (node.box().w - static_cast<double>(label_width)) / 2.0);
        const double y = node.box().y + std::floor(std::max(0.0, node.box().h - 1.0) / 2.0);
        canvas.draw_text(Point{x, y}, node.label(), opts.node_style);
    }

    // 第三段：箭头（落在边框上，必须最后画才不会被覆盖）。
    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        if (route.size() < 2) {
            continue;
        }
        if (edge.arrow() == ArrowStyle::AtTarget || edge.arrow() == ArrowStyle::Both) {
            put_arrow(canvas, route[route.size() - 2], route.back(), opts.edge_style);
        }
        if (edge.arrow() == ArrowStyle::Both) {
            put_arrow(canvas, route[1], route.front(), opts.edge_style);
        }
    }
    return bounds;
}

}  // namespace termchart::graph
