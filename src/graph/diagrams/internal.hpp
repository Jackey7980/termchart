// internal.hpp —— graph/diagrams 的共享渲染骨架（src 私有）
//
// 流程图 / 状态机图 / 思维导图的差别只有三处开关：要不要边框、要不要箭头、要不要
// 边标签；"路由 -> 连线 -> 节点 -> 箭头 -> 边标签"的骨架完全一致。把骨架放在这里
// 一份，三种图各自只剩"布局方式 + 开关组合"。实现为 inline，避免私有 cpp
// （检查器要求每个 .cpp 都有同名公开头）。
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/graph/arrow.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/label_place.hpp"
#include "termchart/graph/port.hpp"
#include "termchart/graph/route/route.hpp"
#include "termchart/graph/shape/shape.hpp"
#include "termchart/graph/wiregrid.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::graph::detail {

struct DiagramStyle {
    RouteOptions route{};
    RouteStyle route_style = RouteStyle::Orthogonal;
    ShapeKind shape = ShapeKind::Box;
    Style node_style{};
    Style edge_style{};
    bool node_border = true;  // false：只画标签（思维导图/节点-连线视图）
    bool arrows = true;
    bool edge_labels = false;
    // false 表示调用方已自行写好折线（思维导图的干线连线），这里不再路由。
    bool auto_route = true;
};

// 轴对齐线段用直线字符，斜线段按走向选对角线字符。
inline char32_t segment_char(Point a, Point b) noexcept {
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

inline void put_arrow(canvas::Canvas& canvas, Point from, Point tip, const Style& style) {
    canvas.put(tip, arrow_head_char(side_towards(from, tip)), style);
}

// 边标签落点：取最长段的中点为锚点，交给 label_place 避开节点框与已有标签。
// 返回 placed == false 表示四个候选位都不可用——调用方应**放弃绘制**，绝不能
// 退而画在节点上（那会把节点标签涂掉）。
inline LabelPlacement label_placement_for(const std::vector<Point>& route,
                                          const std::string& text,
                                          const std::vector<Rect>& occupied) {
    std::size_t best = 1;
    double best_len = -1.0;
    for (std::size_t i = 1; i < route.size(); ++i) {
        const double len = std::fabs(route[i].x - route[i - 1].x) +
                           std::fabs(route[i].y - route[i - 1].y);
        if (len > best_len) {
            best_len = len;
            best = i;
        }
    }
    const Point a = route[best - 1];
    const Point b = route[best];
    const Point mid{(a.x + b.x) / 2.0, (a.y + b.y) / 2.0};
    const auto width = static_cast<std::uint32_t>(std::max(1, util::display_width(text)));
    return place_label(mid, width, 1, occupied);
}

// 已放标签的占用矩形左右各留一格，避免两个标签紧贴成"okreset"。
inline Rect inflate_one_cell(const Rect& rect) noexcept {
    return Rect{rect.x - 1.0, rect.y, rect.w + 2.0, rect.h};
}

// 骨架：路由 -> 连线 -> 节点 -> 箭头 -> 边标签。调用方负责先做好布局。
inline void draw_diagram(Graph& graph, canvas::Canvas& canvas,
                         const DiagramStyle& style) {
    if (graph.nodes().empty()) {
        return;
    }
    if (style.auto_route) {
        if (style.route_style == RouteStyle::Orthogonal) {
            WireGrid grid(canvas.width(), canvas.height());
            (void)route_edges(graph, style.route_style, style.route, &grid);
        } else {
            (void)route_edges(graph, style.route_style, style.route, nullptr);
        }
    }

    // 1) 连线（不画箭头：箭头最后画，否则会被节点边框覆盖）。
    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        for (std::size_t i = 1; i < route.size(); ++i) {
            canvas.draw_line(route[i - 1], route[i],
                             segment_char(route[i - 1], route[i]), style.edge_style);
        }
    }

    // 2) 节点（边框 + 居中标签）。
    for (const Node& node : graph.nodes()) {
        if (style.node_border) {
            draw_shape(canvas, style.shape, node.box(), style.node_style);
        }
        if (node.label().empty()) {
            continue;
        }
        const int label_width = util::display_width(node.label());
        const double x = node.box().x +
                         std::max(0.0, (node.box().w - static_cast<double>(label_width)) / 2.0);
        const double y = node.box().y + std::floor(std::max(0.0, node.box().h - 1.0) / 2.0);
        canvas.draw_text(Point{x, y}, node.label(), style.node_style);
    }

    // 3) 箭头。
    if (style.arrows) {
        for (const Edge& edge : graph.edges()) {
            const std::vector<Point>& route = edge.route();
            if (route.size() < 2) {
                continue;
            }
            if (edge.arrow() == ArrowStyle::AtTarget || edge.arrow() == ArrowStyle::Both) {
                put_arrow(canvas, route[route.size() - 2], route.back(), style.edge_style);
            }
            if (edge.arrow() == ArrowStyle::Both) {
                put_arrow(canvas, route[1], route.front(), style.edge_style);
            }
        }
    }

    // 4) 边标签（状态机的转移事件名等）。
    if (style.edge_labels) {
        std::vector<Rect> occupied;
        occupied.reserve(graph.nodes().size() + graph.edges().size());
        for (const Node& node : graph.nodes()) {
            occupied.push_back(node.box());
        }
        for (const Edge& edge : graph.edges()) {
            if (edge.label().empty() || edge.route().size() < 2) {
                continue;
            }
            const LabelPlacement placement =
                label_placement_for(edge.route(), edge.label(), occupied);
            if (!placement.placed) {
                continue;  // 放不下就不画，宁可缺标签也不涂掉节点
            }
            canvas.draw_text(Point{placement.rect.x, placement.rect.y}, edge.label(),
                             style.edge_style);
            occupied.push_back(inflate_one_cell(placement.rect));
        }
    }
}

}  // namespace termchart::graph::detail
