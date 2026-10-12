// uml_usecase.cpp —— UML 用例图渲染：小人 + 椭圆 + 关联线
#include "termchart/graph/diagrams/uml_usecase.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/graph/port.hpp"
#include "termchart/util/wcwidth.hpp"
#include "internal.hpp"

namespace termchart::graph {

namespace {

constexpr char32_t kHead = U'O';       // 小人头
constexpr char32_t kBody = U'\u002F';  // /（身体与腿由 / | \ 拼）
constexpr char32_t kArm = U'|';
constexpr char32_t kLeg = U'\\';

bool is_actor(const Node& node) { return node.attributes().get("kind") == "actor"; }

// 参与者站哪侧：属性 "side" 优先；缺省按边方向——只作为 from → 左，
// 只作为 to → 右，混合或孤立 → 左。
bool actor_on_right(const Node& node, const Graph& graph) {
    const std::string_view side = node.attributes().get("side");
    if (side == "right") {
        return true;
    }
    if (side == "left") {
        return false;
    }
    bool has_out = false;
    bool has_in = false;
    for (const Edge* edge : graph.edges_of(node.id())) {
        if (edge->from() == node.id()) {
            has_out = true;
        }
        if (edge->to() == node.id()) {
            has_in = true;
        }
    }
    return !has_out && has_in;
}

double center_x(const Rect& box) noexcept { return box.x + box.w / 2.0; }

double center_y(const Rect& box) noexcept { return box.y + std::floor(box.h / 2.0); }

}  // namespace

Rect render_uml_usecase(Graph& graph, canvas::Canvas& canvas, const UmlUsecaseOptions& opts) {
    if (graph.nodes().empty()) {
        return Rect{};
    }

    // ---- 分类与统一尺寸 ----
    std::vector<std::size_t> actors_left;
    std::vector<std::size_t> actors_right;
    std::vector<std::size_t> usecases;
    double actor_w = 6.0;
    double uc_w = std::max(8.0, opts.layout.min_width);
    double uc_h = std::max(3.0, opts.layout.min_height);
    for (std::size_t i = 0; i < graph.nodes().size(); ++i) {
        const Node& node = graph.nodes()[i];
        if (is_actor(node)) {
            (actor_on_right(node, graph) ? actors_right : actors_left).push_back(i);
            actor_w = std::max(actor_w, static_cast<double>(util::display_width(node.label())) + 2.0);
        } else {
            usecases.push_back(i);
            uc_w = std::max(uc_w, static_cast<double>(util::display_width(node.label())) + 4.0);
        }
    }

    const double margin = std::max(1.0, opts.layout.margin);
    const double h_gap = std::max(2.0, opts.h_gap);
    const double v_gap = std::max(1.0, opts.layout.v_gap);

    // ---- 列坐标：左参与者 → 用例 → 右参与者（从左往右排，画布窄了由 canvas 裁剪） ----
    const double left_x = margin;
    const double uc_x = left_x + (actors_left.empty() ? 0.0 : actor_w + h_gap);
    const double right_x = uc_x + uc_w + h_gap;

    // ---- 用例列：纵向排布 ----
    double uc_top = margin;
    for (const std::size_t index : usecases) {
        Node& node = graph.nodes()[index];
        node.set_box(Rect{uc_x, uc_top, uc_w, uc_h});
        uc_top += uc_h + v_gap;
    }
    const double span_top = margin;
    const double span_bottom = usecases.empty() ? margin : uc_top - v_gap - 1.0;

    // ---- 参与者：每侧在用例纵向跨度内均匀分布（无用例时从 margin 起堆叠） ----
    const auto place_actors = [&](std::vector<std::size_t>& list, double x) {
        const std::size_t m = list.size();
        for (std::size_t k = 0; k < m; ++k) {
            Node& node = graph.nodes()[list[k]];
            const double yc = usecases.empty()
                                  ? margin + 1.0 + static_cast<double>(k) * (4.0 + v_gap)
                                  : span_top +
                                        (span_bottom - span_top) *
                                            (static_cast<double>(k) + 1.0) /
                                            (static_cast<double>(m) + 1.0);
            // 小人 3 行 + 名字 1 行；以 yc 为小人中行。
            const double top = std::max(0.0, yc - 1.0);
            node.set_box(Rect{x + actor_w / 2.0 - 1.0, top, 3.0, 3.0});
        }
    };
    place_actors(actors_left, left_x);
    place_actors(actors_right, right_x);

    // ---- 1) 关联线（先画线，节点后画覆盖线头） ----
    for (const Edge& edge : graph.edges()) {
        const Node* from = graph.find(edge.from());
        const Node* to = graph.find(edge.to());
        if (from == nullptr || to == nullptr) {
            continue;
        }
        const Point a{center_x(from->box()), center_y(from->box())};
        const Point b{center_x(to->box()), center_y(to->box())};
        const Point pa = port_anchor(from->box(), side_towards(a, b));
        const Point pb = port_anchor(to->box(), side_towards(b, a));
        canvas.draw_line(pa, pb, detail::segment_char(pa, pb), opts.edge_style);
    }

    // ---- 2) 节点 ----
    for (const Node& node : graph.nodes()) {
        const Rect box = node.box();
        if (node.label().empty()) {
            continue;
        }
        const int label_w = util::display_width(node.label());
        const Style& style = is_actor(node) ? opts.actor_style : opts.usecase_style;
        if (is_actor(node)) {
            const double cx = center_x(box);
            const double top = box.y;
            canvas.put(Point{cx, top}, kHead, style);
            canvas.put(Point{cx - 1.0, top + 1.0}, kBody, style);
            canvas.put(Point{cx, top + 1.0}, kArm, style);
            canvas.put(Point{cx + 1.0, top + 1.0}, kLeg, style);
            canvas.put(Point{cx - 1.0, top + 2.0}, kBody, style);
            canvas.put(Point{cx + 1.0, top + 2.0}, kLeg, style);
            canvas.draw_text(
                Point{cx - static_cast<double>(label_w) / 2.0, top + 3.0}, node.label(), style);
        } else {
            draw_shape(canvas, ShapeKind::Ellipse, box, style);
            canvas.draw_text(
                Point{center_x(box) - static_cast<double>(label_w) / 2.0, center_y(box)},
                node.label(), style);
        }
    }
    return graph.bounds();
}

}  // namespace termchart::graph
