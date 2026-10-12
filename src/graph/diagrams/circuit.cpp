// circuit.cpp —— 电路图实现
#include "termchart/graph/diagrams/circuit.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/primitives.hpp"
#include "termchart/graph/route/junction.hpp"
#include "termchart/graph/shape/shape.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::graph {
namespace {

// 元件外框尺寸：电阻/电容/电池 5×3，地 5×5。
Rect cell_box(const CircuitComponent& c) noexcept {
    const double h = (c.kind == ComponentKind::Ground) ? 5.0 : 3.0;
    return Rect{c.at.x, c.at.y, 5.0, h};
}

bool is_horizontal_side(PortSide side) noexcept {
    return side == PortSide::East || side == PortSide::West;
}

// 折线的每一格是否都不落进任何元件框（首末格 a/b 是引脚，落在边框上，放行）。
bool path_clear(const std::vector<Point>& path, const std::vector<Rect>& boxes,
                Point a, Point b) {
    for (std::size_t i = 1; i < path.size(); ++i) {
        const std::vector<Point> seg = canvas::line_points(path[i - 1], path[i]);
        for (const Point& c : seg) {
            if (c == a || c == b) {
                continue;
            }
            for (const Rect& box : boxes) {
                if (box.contains(c)) {
                    return false;
                }
            }
        }
    }
    return true;
}

// 正交走线：从 from 引脚的朝向轴先走；中段列/行从中点起向两侧逐格找空档
// （避让元件框）。全失败退化为 L 形——宁可直穿也不消失。
std::vector<Point> route_orthogonal(Point a, PortSide sa, Point b, PortSide sb,
                                    const std::vector<Rect>& boxes) {
    if (a == b) {
        return {a};
    }
    if (a.y == b.y || a.x == b.x) {
        return {a, b};  // 已对齐：直线本身就是合法的正交走线
    }
    const bool a_horiz = is_horizontal_side(sa);
    const bool b_horiz = is_horizontal_side(sb);
    const bool horiz_first =
        a_horiz ? true : (b_horiz ? false
                                  : std::fabs(b.x - a.x) >= std::fabs(b.y - a.y));
    if (horiz_first) {
        const double mid = (std::min(a.x, b.x) + std::max(a.x, b.x)) / 2.0;
        for (int k = 0; k <= 8; ++k) {
            for (int s = 0; s < 2; ++s) {
                if (k == 0 && s == 1) {
                    continue;
                }
                const double mx =
                    mid + (s == 0 ? 1.0 : -1.0) * static_cast<double>(k);
                const std::vector<Point> path{a, Point{mx, a.y}, Point{mx, b.y}, b};
                if (path_clear(path, boxes, a, b)) {
                    return path;
                }
            }
        }
        return {a, Point{b.x, a.y}, b};  // L 形兜底
    }
    const double mid = (std::min(a.y, b.y) + std::max(a.y, b.y)) / 2.0;
    for (int k = 0; k <= 8; ++k) {
        for (int s = 0; s < 2; ++s) {
            if (k == 0 && s == 1) {
                continue;
            }
            const double my = mid + (s == 0 ? 1.0 : -1.0) * static_cast<double>(k);
            const std::vector<Point> path{a, Point{a.x, my}, Point{b.x, my}, b};
            if (path_clear(path, boxes, a, b)) {
                return path;
            }
        }
    }
    return {a, Point{a.x, b.y}, b};  // L 形兜底
}

// 折角收口：route 的内部点（拐点或共线中点）按四向连通性换成 T 形/角字符。
void fixup_corners(canvas::Canvas& canvas, const std::vector<Point>& route,
                   const Style& style) {
    for (std::size_t i = 1; i + 1 < route.size(); ++i) {
        const Point prev = route[i - 1];
        const Point cur = route[i];
        const Point next = route[i + 1];
        const bool up = prev.y < cur.y || next.y < cur.y;
        const bool down = prev.y > cur.y || next.y > cur.y;
        const bool left = prev.x < cur.x || next.x < cur.x;
        const bool right = prev.x > cur.x || next.x > cur.x;
        const char32_t cp = junction_char(up, down, left, right);
        if (cp == U' ') {
            continue;
        }
        canvas.put(cur, cp, style);
    }
}

void draw_glyph(canvas::Canvas& canvas, ComponentKind kind, const Rect& box,
                const Style& style) {
    const double x = box.x;
    const double y = box.y;
    switch (kind) {
        case ComponentKind::Resistor:
            draw_shape(canvas, ShapeKind::Box, box, style);
            break;
        case ComponentKind::Capacitor:
            canvas.put(Point{x, y + 1.0}, U'\u2500', style);
            canvas.put(Point{x + 1.0, y + 1.0}, U'\u2502', style);
            canvas.put(Point{x + 3.0, y + 1.0}, U'\u2502', style);
            canvas.put(Point{x + 4.0, y + 1.0}, U'\u2500', style);
            break;
        case ComponentKind::Battery:
            canvas.put(Point{x, y + 1.0}, U'\u2500', style);
            canvas.put(Point{x + 4.0, y + 1.0}, U'\u2500', style);
            for (int r = 0; r < 3; ++r) {  // 长板
                canvas.put(Point{x + 1.0, y + static_cast<double>(r)}, U'\u2502', style);
            }
            canvas.put(Point{x + 3.0, y + 1.0}, U'\u2502', style);  // 短板
            break;
        case ComponentKind::Ground:
            canvas.put(Point{x + 2.0, y + 1.0}, U'\u2502', style);
            for (int c = 1; c <= 3; ++c) {
                canvas.put(Point{x + static_cast<double>(c), y + 2.0}, U'\u2500', style);
            }
            canvas.put(Point{x + 2.0, y + 3.0}, U'\u2500', style);
            canvas.put(Point{x + 2.0, y + 4.0}, U'\u00B7', style);
            break;
    }
}

}  // namespace

Rect render_circuit(const std::vector<CircuitComponent>& parts,
                    const std::vector<CircuitWire>& wires, canvas::Canvas& canvas,
                    const CircuitOptions& opts) {
    if (parts.empty()) {
        return Rect{};
    }

    std::vector<Rect> boxes;
    boxes.reserve(parts.size());
    for (const CircuitComponent& c : parts) {
        boxes.push_back(cell_box(c));
    }

    // 1) 连线（先画：元件符号后画，压住误入边框的线头）。
    for (const CircuitWire& w : wires) {
        if (w.from.component >= parts.size() || w.to.component >= parts.size()) {
            continue;
        }
        const Point a = port_anchor(boxes[w.from.component], w.from.side);
        const Point b = port_anchor(boxes[w.to.component], w.to.side);
        const std::vector<Point> route =
            route_orthogonal(a, w.from.side, b, w.to.side, boxes);
        for (std::size_t i = 1; i < route.size(); ++i) {
            canvas.draw_line(route[i - 1], route[i],
                             route[i - 1].y == route[i].y ? U'\u2500' : U'\u2502',
                             opts.wire_style);
        }
        fixup_corners(canvas, route, opts.wire_style);
    }

    // 2) 元件符号 + 标签。
    Rect bounds{};
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const Rect box = boxes[i];
        draw_glyph(canvas, parts[i].kind, box, opts.glyph_style);
        const std::string& label = parts[i].label;
        if (!label.empty()) {
            const double w = static_cast<double>(util::display_width(label));
            if (parts[i].kind == ComponentKind::Resistor && w <= 3.0) {
                // 框内：内部 3 列里居中。
                const double x = box.x + 1.0 + std::floor((3.0 - w) / 2.0);
                canvas.draw_text(Point{x, box.y + 1.0}, label, opts.label_style);
                bounds = bounds.union_with(Rect{x, box.y + 1.0, w, 1.0});
            } else {
                // 框下：居中（可为负偏移，越界由 canvas 裁剪）。
                const double x = box.x + std::floor((box.w - w) / 2.0);
                const double y = box.bottom();
                canvas.draw_text(Point{x, y}, label, opts.label_style);
                bounds = bounds.union_with(Rect{x, y, w, 1.0});
            }
        }
        bounds = bounds.union_with(box);
    }
    return bounds;
}

}  // namespace termchart::graph
