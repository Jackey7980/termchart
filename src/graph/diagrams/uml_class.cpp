// uml_class.cpp —— UML 类图渲染：三栏类框 + 空心三角泛化
#include "termchart/graph/diagrams/uml_class.hpp"

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

namespace {

constexpr char32_t kHorizontal = U'\u2500';      // ─ 分隔线横段
constexpr char32_t kTeeLeft = U'\u2524';         // ┤ 分隔线右端
constexpr char32_t kTeeRight = U'\u251C';        // ├ 分隔线左端
constexpr char32_t kGeneralization = U'\u25C1';  // ◁ 指向基类的空心三角

// 泛化边（默认）在 from（基类）端画空心三角；"assoc" 边按普通箭头处理。
bool is_generalization(const Edge& edge) {
    return edge.attributes().get("kind") != "assoc";
}

// "attrs" / "ops" 的多行内容按 '\n' 切开。
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

// 类框必须装得下三栏内容：宽 = 最宽行 + 左右各 2 列；高 = 上下边框 + 类名 +
// 分隔线 + 属性 +（有操作时）分隔线 + 操作。取所有类框的最大值，整图统一尺寸。
void enlarge_for_content(Graph& graph, LayoutOptions& layout) {
    double width = layout.min_width;
    double height = layout.min_height;
    for (const Node& node : graph.nodes()) {
        int widest = util::display_width(node.label());
        const auto attrs = split_lines(node.attributes().get("attrs"));
        const auto ops = split_lines(node.attributes().get("ops"));
        for (const std::string& line : attrs) {
            widest = std::max(widest, util::display_width(line));
        }
        for (const std::string& line : ops) {
            widest = std::max(widest, util::display_width(line));
        }
        width = std::max(width, static_cast<double>(widest) + 4.0);
        const double rows = 4.0 + static_cast<double>(attrs.size()) +
                            static_cast<double>(ops.size()) +
                            (ops.empty() ? 0.0 : 1.0);
        height = std::max(height, rows);
    }
    layout.min_width = width;
    layout.min_height = height;
}

void put_centered(canvas::Canvas& canvas, const Rect& box, const std::string& text,
                  double row, const Style& style) {
    if (text.empty()) {
        return;
    }
    const int text_w = util::display_width(text);
    const double x = box.x + std::max(2.0, (box.w - static_cast<double>(text_w)) / 2.0);
    canvas.draw_text(Point{x, row}, text, style);
}

// 分隔线：左 ├ + 横线 + 右 ┤，与左右边框连成一体。
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

void draw_class_box(canvas::Canvas& canvas, const Node& node, const UmlClassOptions& opts) {
    const Rect box = node.box();
    draw_shape(canvas, ShapeKind::Box, box, opts.border_style);

    const auto attrs = split_lines(node.attributes().get("attrs"));
    const auto ops = split_lines(node.attributes().get("ops"));
    const double bottom = box.bottom() - 1.0;

    double row = box.y + 1.0;
    put_centered(canvas, box, node.label(), row, opts.name_style);
    row += 1.0;
    if (row >= bottom) {
        return;
    }
    draw_separator(canvas, box, row, opts.border_style);
    row += 1.0;
    for (const std::string& line : attrs) {
        if (row >= bottom) {
            return;
        }
        canvas.draw_text(Point{box.x + 2.0, row}, line, opts.member_style);
        row += 1.0;
    }
    if (ops.empty() || row >= bottom) {
        return;
    }
    draw_separator(canvas, box, row, opts.border_style);
    row += 1.0;
    for (const std::string& line : ops) {
        if (row >= bottom) {
            return;
        }
        canvas.draw_text(Point{box.x + 2.0, row}, line, opts.member_style);
        row += 1.0;
    }
}

void draw_edge_ends(canvas::Canvas& canvas, const Graph& graph, const UmlClassOptions& opts) {
    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        if (route.size() < 2) {
            continue;
        }
        if (opts.generalization_triangles && is_generalization(edge)) {
            // 空心三角贴在基类（from）端边框上，指向基类。最后画：先画会被
            // 类框边框覆盖（三角占的正是边框格）。
            canvas.put(route.front(), kGeneralization, opts.edge_style);
        } else {
            detail::put_arrow(canvas, route[route.size() - 2], route.back(), opts.edge_style);
        }
    }
}

}  // namespace

Rect render_uml_class(Graph& graph, canvas::Canvas& canvas, const UmlClassOptions& opts) {
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

    // 1) 连线（不画端头：类框与三角都要后画，避免被覆盖）。
    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        for (std::size_t i = 1; i < route.size(); ++i) {
            canvas.draw_line(route[i - 1], route[i],
                             detail::segment_char(route[i - 1], route[i]), opts.edge_style);
        }
    }
    // 2) 类框（三栏内容）。
    for (const Node& node : graph.nodes()) {
        draw_class_box(canvas, node, opts);
    }
    // 3) 边端（三角/箭头）。
    draw_edge_ends(canvas, graph, opts);
    return bounds;
}

}  // namespace termchart::graph
