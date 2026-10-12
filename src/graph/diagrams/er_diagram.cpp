// er_diagram.cpp —— ER 图渲染：实体表框 + 鸭掌记号 + 关系名标签
#include "termchart/graph/diagrams/er_diagram.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/util/wcwidth.hpp"
#include "internal.hpp"

namespace termchart::graph {

ErCardinality parse_er_cardinality(std::string_view text) noexcept {
    if (text == "N" || text == "n" || text == "M" || text == "m" || text == "many" ||
        text == "*") {
        return ErCardinality::Many;
    }
    if (text == "0..1" || text == "0-1" || text == "o|") {
        return ErCardinality::ZeroOrOne;
    }
    return ErCardinality::One;
}

namespace {

constexpr char32_t kHorizontal = U'\u2500';
constexpr char32_t kTeeLeft = U'\u2524';
constexpr char32_t kTeeRight = U'\u251C';
constexpr char32_t kTickV = U'\u2502';     // │ 垂直短杠（横线上的"一"记号）
constexpr char32_t kTickH = U'\u2500';     // ─ 水平短杠（竖线上的"一"记号）
constexpr char32_t kFootLeft = U'<';       // 鸭掌：开口朝左
constexpr char32_t kFootRight = U'>';      // 开口朝右
constexpr char32_t kFootUp = U'\u2228';    // ∨ 开口朝上
constexpr char32_t kFootDown = U'\u2227';  // ∧ 开口朝下
constexpr char32_t kCircle = U'\u25CB';    // ○ 零

ErCardinality card_of(const Edge& edge, bool from_end) {
    return parse_er_cardinality(from_end ? edge.attributes().get("card_from")
                                         : edge.attributes().get("card_to"));
}

std::vector<std::string> split_lines(std::string_view text) {
    std::vector<std::string> lines;
    if (text.empty()) {
        return lines;
    }
    std::size_t start = 0;
    while (true) {
        const std::size_t nl = text.find('\n', start);
        if (nl == std::string_view::npos) {
            lines.emplace_back(text.substr(start));
            break;
        }
        lines.emplace_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    return lines;
}

// 实体表框装得下内容：宽 = 最宽行 + 左右各 2 列；高 = 上下边框 + 实体名 +
// 分隔线 + 字段。
void enlarge_for_content(Graph& graph, LayoutOptions& layout) {
    double width = layout.min_width;
    double height = layout.min_height;
    for (const Node& node : graph.nodes()) {
        int widest = util::display_width(node.label());
        const auto fields = split_lines(node.attributes().get("fields"));
        for (const std::string& line : fields) {
            widest = std::max(widest, util::display_width(line));
        }
        width = std::max(width, static_cast<double>(widest) + 4.0);
        height = std::max(height, 4.0 + static_cast<double>(fields.size()));
    }
    layout.min_width = width;
    layout.min_height = height;
}

void draw_separator(canvas::Canvas& canvas, const Rect& box, double row, const Style& style) {
    const auto left = static_cast<std::int64_t>(std::lround(box.x));
    const auto width = static_cast<std::int64_t>(std::lround(box.w));
    if (width < 2) {
        return;
    }
    for (std::int64_t i = 0; i + 1 < width; ++i) {
        const char32_t cp = (i == 0) ? kTeeRight : kHorizontal;
        canvas.put(Point{static_cast<double>(left + i), row}, cp, style);
    }
    canvas.put(Point{static_cast<double>(left + width - 1), row}, kTeeLeft, style);
}

void draw_entity_box(canvas::Canvas& canvas, const Node& node, const ErDiagramOptions& opts) {
    const Rect box = node.box();
    draw_shape(canvas, ShapeKind::Box, box, opts.entity_style);
    const auto fields = split_lines(node.attributes().get("fields"));
    const double bottom = box.bottom() - 1.0;

    double row = box.y + 1.0;
    if (!node.label().empty()) {
        const int name_w = util::display_width(node.label());
        canvas.draw_text(
            Point{box.x + std::max(2.0, (box.w - static_cast<double>(name_w)) / 2.0), row},
            node.label(), opts.name_style);
    }
    row += 1.0;
    if (row >= bottom) {
        return;
    }
    draw_separator(canvas, box, row, opts.entity_style);
    row += 1.0;
    for (const std::string& line : fields) {
        if (row >= bottom) {
            return;
        }
        canvas.draw_text(Point{box.x + 2.0, row}, line, opts.field_style);
        row += 1.0;
    }
}

// 在一端的实体边框外画基数记号。E = 边框上的锚点，A = 折线上相邻点（远离实体）。
void draw_cardinality(canvas::Canvas& canvas, Point anchor, Point adjacent,
                      ErCardinality card, const Style& style) {
    const double dx = adjacent.x - anchor.x;
    const double dy = adjacent.y - anchor.y;
    const bool horizontal = std::fabs(dx) >= std::fabs(dy);
    // p1 紧贴实体边框，p2 再远一格。
    const Point p1{anchor.x + (horizontal ? (dx > 0 ? 1.0 : -1.0) : 0.0),
                   anchor.y + (horizontal ? 0.0 : (dy > 0 ? 1.0 : -1.0))};
    const Point p2{anchor.x + (horizontal ? (dx > 0 ? 2.0 : -2.0) : 0.0),
                   anchor.y + (horizontal ? 0.0 : (dy > 0 ? 2.0 : -2.0))};
    const char32_t tick = horizontal ? kTickV : kTickH;
    switch (card) {
        case ErCardinality::One:
            canvas.put(p1, tick, style);
            canvas.put(p2, tick, style);
            break;
        case ErCardinality::Many: {
            // 鸭掌：尖连关系线、开口朝实体边框。
            char32_t foot = kFootLeft;
            if (horizontal) {
                foot = (dx > 0) ? kFootRight : kFootLeft;
            } else {
                foot = (dy > 0) ? kFootUp : kFootDown;
            }
            canvas.put(p1, foot, style);
            break;
        }
        case ErCardinality::ZeroOrOne:
            canvas.put(p1, tick, style);
            canvas.put(p2, kCircle, style);
            break;
    }
}

}  // namespace

Rect render_er_diagram(Graph& graph, canvas::Canvas& canvas, const ErDiagramOptions& opts) {
    if (graph.nodes().empty()) {
        return Rect{};
    }
    LayoutOptions layout = opts.layout;
    enlarge_for_content(graph, layout);
    const Rect bounds = layout_layered(graph, layout);

    if (opts.route_style == RouteStyle::Orthogonal) {
        WireGrid grid(canvas.width(), canvas.height());
        (void)route_edges(graph, RouteStyle::Orthogonal, opts.route, &grid);
    } else {
        (void)route_edges(graph, RouteStyle::Straight, opts.route, nullptr);
    }

    // 1) 关系线（无箭头）。
    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        for (std::size_t i = 1; i < route.size(); ++i) {
            canvas.draw_line(route[i - 1], route[i],
                             detail::segment_char(route[i - 1], route[i]), opts.edge_style);
        }
    }

    // 2) 实体表框。
    for (const Node& node : graph.nodes()) {
        draw_entity_box(canvas, node, opts);
    }

    // 3) 鸭掌记号（先于标签、后于表框：记号压在紧贴边框的线格上）。
    if (opts.draw_cardinality) {
        for (const Edge& edge : graph.edges()) {
            const std::vector<Point>& route = edge.route();
            if (route.size() < 2) {
                continue;
            }
            draw_cardinality(canvas, route.front(), route[1],
                             card_of(edge, true), opts.edge_style);
            draw_cardinality(canvas, route.back(), route[route.size() - 2],
                             card_of(edge, false), opts.edge_style);
        }
    }

    // 4) 关系名标签（label_place 避让实体框；放不下宁可不画）。
    {
        std::vector<Rect> occupied;
        occupied.reserve(graph.nodes().size());
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
        }
    }
    return bounds;
}

}  // namespace termchart::graph
