// social_network.cpp —— 社交网络图渲染：环形布局 + 共享绘制骨架
#include "termchart/graph/diagrams/social_network.hpp"

#include <cmath>
#include <cstddef>

#include "termchart/canvas/canvas.hpp"
#include "internal.hpp"

namespace termchart::graph {

namespace {

constexpr double kPi = 3.14159265358979323846;

}  // namespace

Rect render_social_network(Graph& graph, canvas::Canvas& canvas,
                           const SocialNetworkOptions& opts) {
    if (graph.nodes().empty()) {
        return Rect{};
    }

    // 等弦间隔：相邻两节点的圆心距 2r·sin(π/n) 要塞得下节点 + 间距。
    double max_w = 1.0;
    double max_h = 1.0;
    for (const Node& node : graph.nodes()) {
        const Rect size = node_box_for(node, opts.layout);
        max_w = std::max(max_w, size.w);
        max_h = std::max(max_h, size.h);
    }
    const std::size_t count = graph.node_count();
    double radius = opts.radius;
    if (radius <= 0.0) {
        if (count >= 2) {
            const double need = std::max(max_w + opts.layout.h_gap,
                                         max_h + opts.layout.v_gap);
            radius = need / (2.0 * std::sin(kPi / static_cast<double>(count)));
        } else {
            radius = 0.0;
        }
    }
    const double margin = opts.layout.margin;
    const double cx = margin + max_w / 2.0 + radius;
    const double cy = margin + max_h / 2.0 + radius;

    Rect bounds{};
    for (std::size_t i = 0; i < count; ++i) {
        Node& node = graph.nodes()[i];
        const Rect size = node_box_for(node, opts.layout);
        double x = cx;
        double y = cy;
        if (count >= 2) {
            const double angle =
                2.0 * kPi * static_cast<double>(i) / static_cast<double>(count);
            x = cx + radius * std::cos(angle);
            y = cy + radius * std::sin(angle);
        }
        node.set_box(Rect{x - size.w / 2.0, y - size.h / 2.0, size.w, size.h});
        bounds = bounds.union_with(node.box());
    }

    detail::DiagramStyle style;
    style.route_style = RouteStyle::Straight;  // 直线连线：环上节点互相可见
    style.shape = ShapeKind::Box;
    style.node_style = opts.node_style;
    style.edge_style = opts.edge_style;
    style.node_border = opts.node_border;
    style.arrows = opts.arrows;
    style.edge_labels = false;
    detail::draw_diagram(graph, canvas, style);
    return bounds;
}

}  // namespace termchart::graph
