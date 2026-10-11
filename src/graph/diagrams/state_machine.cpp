// state_machine.cpp —— 状态机图渲染
#include "termchart/graph/diagrams/state_machine.hpp"

#include <algorithm>
#include <cmath>

#include "termchart/canvas/canvas.hpp"
#include "internal.hpp"

namespace termchart::graph {

namespace {

constexpr char32_t kInitialMarker = U'\u25CF';  // ● 初始状态标记
constexpr char32_t kFinalMarker = U'\u25CE';    // ◎ 终止状态标记
constexpr char32_t kMarkerLink = U'\u2500';     // ─ 初始标记引入线

}  // namespace

Rect render_state_machine(Graph& graph, canvas::Canvas& canvas,
                          const StateMachineOptions& opts) {
    if (graph.nodes().empty()) {
        return Rect{};
    }
    // 起止标记画在状态框**外侧**，所以左侧要多留 2 列：否则最左状态（列 1）的
    // ● 落在列 -1，而 canvas 对负坐标是裁剪而非折回，标记会凭空消失。
    LayoutOptions layout = opts.layout;
    if (opts.mark_initial_final) {
        layout.margin += 2.0;
    }
    const Rect bounds = layout_layered(graph, layout);

    detail::DiagramStyle style;
    style.route = opts.route;
    style.route_style = opts.route_style;
    style.shape = opts.shape;
    style.node_style = opts.node_style;
    style.edge_style = opts.edge_style;
    style.node_border = true;
    style.arrows = true;
    style.edge_labels = true;  // 转移事件名
    detail::draw_diagram(graph, canvas, style);

    // 起止标记最后画：它们压在边框外的格上，先画会被别的元素覆盖。
    if (opts.mark_initial_final) {
        for (const Node& node : graph.nodes()) {
            const Rect box = node.box();
            const double center_y =
                box.y + std::floor(std::max(0.0, box.h - 1.0) / 2.0);
            if (node.attributes().get("initial") == "1") {
                canvas.put(Point{box.x - 2.0, center_y}, kInitialMarker, opts.edge_style);
                // 引入线只画到边框**前一格**，避免覆盖状态框的左边框。
                canvas.put(Point{box.x - 1.0, center_y}, kMarkerLink, opts.edge_style);
            }
            if (node.attributes().get("final") == "1") {
                canvas.put(Point{box.right(), center_y}, kFinalMarker, opts.node_style);
            }
        }
    }
    return bounds;
}

}  // namespace termchart::graph
