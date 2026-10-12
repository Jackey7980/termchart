// uml_activity.cpp —— UML 活动图渲染：分层布局 + 逐节点形状 + 起止标记
#include "termchart/graph/diagrams/uml_activity.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "termchart/canvas/canvas.hpp"
#include "termchart/util/wcwidth.hpp"
#include "internal.hpp"

namespace termchart::graph {

namespace {

constexpr char32_t kStartMarker = U'\u25CF';  // ● 起点
constexpr char32_t kEndMarker = U'\u25CE';    // ◎ 终点
constexpr char32_t kMarkerLink = U'\u2500';   // ─ 引入线

bool is_decision(const Node& node) { return node.attributes().get("kind") == "decision"; }

// 没有显式起止属性时，自动标记：首个人度 0 的节点为起点、首个出度 0 的为终点。
// （UML 只允许一个起点/活动终点，取登记顺序的第一个即可。）
void auto_mark_start_end(Graph& graph) {
    bool has_initial = false;
    bool has_final = false;
    for (const Node& node : graph.nodes()) {
        has_initial = has_initial || node.attributes().get("initial") == "1";
        has_final = has_final || node.attributes().get("final") == "1";
    }
    if (!has_initial) {
        for (Node& node : graph.nodes()) {
            if (graph.in_degree(node.id()) == 0) {
                node.attributes().set("initial", "1");
                break;
            }
        }
    }
    if (!has_final) {
        for (Node& node : graph.nodes()) {
            if (graph.out_degree(node.id()) == 0) {
                node.attributes().set("final", "1");
                break;
            }
        }
    }
}

}  // namespace

Rect render_uml_activity(Graph& graph, canvas::Canvas& canvas, const UmlActivityOptions& opts) {
    if (graph.nodes().empty()) {
        return Rect{};
    }
    if (opts.mark_start_end) {
        auto_mark_start_end(graph);
    }

    // 起止标记画在框外，左缘要多留 2 列（负坐标是裁剪不是折回）。
    LayoutOptions layout = opts.layout;
    if (opts.mark_start_end) {
        layout.margin += 2.0;
    }
    const Rect bounds = layout_layered(graph, layout);

    if (opts.route_style == RouteStyle::Orthogonal) {
        WireGrid grid(canvas.width(), canvas.height());
        (void)route_edges(graph, RouteStyle::Orthogonal, opts.route, &grid);
    } else {
        (void)route_edges(graph, RouteStyle::Straight, opts.route, nullptr);
    }

    // 1) 连线（箭头最后画）。
    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        for (std::size_t i = 1; i < route.size(); ++i) {
            canvas.draw_line(route[i - 1], route[i],
                             detail::segment_char(route[i - 1], route[i]), opts.edge_style);
        }
    }

    // 2) 节点：动作 = 圆角框，判定 = 菱形；标签居中。
    for (const Node& node : graph.nodes()) {
        const bool decision = is_decision(node);
        draw_shape(canvas, decision ? ShapeKind::Diamond : ShapeKind::Rounded, node.box(),
                   decision ? opts.decision_style : opts.node_style);
        if (node.label().empty()) {
            continue;
        }
        const Rect box = node.box();
        const int label_w = util::display_width(node.label());
        const double x = box.x + std::max(1.0, (box.w - static_cast<double>(label_w)) / 2.0);
        const double y = box.y + std::floor(std::max(0.0, box.h - 1.0) / 2.0);
        canvas.draw_text(Point{x, y}, node.label(), opts.node_style);
    }

    // 3) 箭头。
    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        if (route.size() < 2) {
            continue;
        }
        if (edge.arrow() == ArrowStyle::AtTarget || edge.arrow() == ArrowStyle::Both) {
            detail::put_arrow(canvas, route[route.size() - 2], route.back(), opts.edge_style);
        }
        if (edge.arrow() == ArrowStyle::Both) {
            detail::put_arrow(canvas, route[1], route.front(), opts.edge_style);
        }
    }

    // 4) 守卫条件等边标签（复用 label_place 避让：放不下宁可不画）。
    {
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
                detail::label_placement_for(edge.route(), edge.label(), occupied);
            if (!placement.placed) {
                continue;
            }
            canvas.draw_text(Point{placement.rect.x, placement.rect.y}, edge.label(),
                             opts.edge_style);
            occupied.push_back(detail::inflate_one_cell(placement.rect));
        }
    }

    // 5) 起止标记（最后画：压在框外的格上）。
    if (opts.mark_start_end) {
        for (const Node& node : graph.nodes()) {
            const Rect box = node.box();
            const double center_y = box.y + std::floor(std::max(0.0, box.h - 1.0) / 2.0);
            if (node.attributes().get("initial") == "1") {
                canvas.put(Point{box.x - 2.0, center_y}, kStartMarker, opts.edge_style);
                canvas.put(Point{box.x - 1.0, center_y}, kMarkerLink, opts.edge_style);
            }
            if (node.attributes().get("final") == "1") {
                canvas.put(Point{box.right(), center_y}, kEndMarker, opts.edge_style);
            }
        }
    }
    return bounds;
}

}  // namespace termchart::graph
