// arc.cpp —— 弧线路由：三次贝塞尔弧 + 自环绕行
//
// 普通边为什么用三次贝塞尔而不是"圆周弧"：弦上的两个锚点未必水平/垂直，真圆需要
// 先定圆心与半径，格粒度下还会因为字符格 1:2 的宽高比被压扁。三次贝塞尔两个控制点
// 沿弦方向外推 1/3 弦长（保证从锚点出发先朝框**外**走）、再垂直于弦隆起弦长的一半，
// 既贴合"跨列半圆"的观感，又不会一开始就贴着边框列切进节点。
//
// 自环为什么必须先水平离框：锚点落在节点边框上，若第一步就斜着走，取整后头几格会
// 停在边框列上随行号变化——那一列的下方全在框内，等于直接穿框。先横着出框一格
// （x ≥ box.right()）再转弯，整圈就都在框外了；框内的标签因此天然安全。
#include "termchart/graph/route/arc.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/canvas/primitives.hpp"
#include "termchart/graph/port.hpp"
#include "termchart/graph/route/route.hpp"

namespace termchart::graph {

namespace {

constexpr double kMaxGridCoord = 1.0e9;
constexpr double kSamplesPerCell = 4.0;

bool finite_point(Point p) noexcept {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::fabs(p.x) <= kMaxGridCoord &&
           std::fabs(p.y) <= kMaxGridCoord;
}

Point snap(Point p) noexcept { return Point{std::round(p.x), std::round(p.y)}; }

double length(Point d) noexcept { return std::hypot(d.x, d.y); }

Point unit(Point d) noexcept {
    const double len = length(d);
    if (!(len > 0.0)) {
        return Point{0.0, 0.0};
    }
    return Point{d.x / len, d.y / len};
}

std::size_t sample_steps(double span) noexcept {
    if (!(span > 0.0)) {
        return 2;
    }
    const double raw = std::ceil(span * kSamplesPerCell) + 2.0;
    if (raw >= 8192.0) {
        return 8192;
    }
    return static_cast<std::size_t>(raw);
}

Point quadratic(Point p0, Point p1, Point p2, double t) noexcept {
    const double u = 1.0 - t;
    return Point{u * u * p0.x + 2.0 * u * t * p1.x + t * t * p2.x,
                 u * u * p0.y + 2.0 * u * t * p1.y + t * t * p2.y};
}

Point cubic(Point p0, Point p1, Point p2, Point p3, double t) noexcept {
    const double u = 1.0 - t;
    const double w0 = u * u * u;
    const double w1 = 3.0 * u * u * t;
    const double w2 = 3.0 * u * t * t;
    const double w3 = t * t * t;
    return Point{p0.x * w0 + p1.x * w1 + p2.x * w2 + p3.x * w3,
                 p0.y * w0 + p1.y * w1 + p2.y * w2 + p3.y * w3};
}

std::vector<Point> cubic_samples(Point p0, Point p1, Point p2, Point p3) {
    const double chord = length(p3 - p0);
    const double bow = length(p1 - p0) + length(p2 - p1) + length(p3 - p2);
    const std::size_t steps = sample_steps(std::max(chord, bow * 0.5));
    std::vector<Point> out;
    out.reserve(steps + 1);
    for (std::size_t k = 0; k <= steps; ++k) {
        const double t = static_cast<double>(k) / static_cast<double>(steps);
        out.push_back(cubic(p0, p1, p2, p3, t));
    }
    return out;
}

// 把折角换成二次贝塞尔（"切角"）。只在自环上用，半径固定 1 格：再大就会把内侧格
// 推到节点框里。
std::vector<Point> round_corners(const std::vector<Point>& poly, double radius) {
    if (poly.size() < 3 || !(radius > 0.0)) {
        return poly;
    }
    std::vector<Point> out;
    out.push_back(poly.front());
    for (std::size_t i = 1; i + 1 < poly.size(); ++i) {
        const Point prev = poly[i - 1];
        const Point cur = poly[i];
        const Point next = poly[i + 1];
        const double l1 = length(cur - prev);
        const double l2 = length(next - cur);
        if (!(l1 > 0.0) || !(l2 > 0.0)) {
            out.push_back(cur);
            continue;
        }
        const Point d1 = unit(cur - prev);
        const Point d2 = unit(next - cur);
        const double r = std::min({radius, l1 * 0.5, l2 * 0.5});
        const Point s{cur.x - d1.x * r, cur.y - d1.y * r};
        const Point e{cur.x + d2.x * r, cur.y + d2.y * r};
        const std::size_t steps = sample_steps(r * 2.0);
        for (std::size_t k = 0; k < steps; ++k) {
            const double t = static_cast<double>(k) / static_cast<double>(steps);
            out.push_back(quadratic(s, cur, e, t));
        }
    }
    out.push_back(poly.back());
    return out;
}

// 相邻格之间补足 8 连通步：复用唯一的逐格连线原语（canvas::line_points）。
std::vector<Point> connect_cells(const std::vector<Point>& cells) {
    std::vector<Point> out;
    out.reserve(cells.size());
    for (const Point& c : cells) {
        if (out.empty()) {
            out.push_back(c);
            continue;
        }
        if (out.back() == c) {
            continue;
        }
        const std::vector<Point> seg = canvas::line_points(out.back(), c);
        for (std::size_t k = 1; k < seg.size(); ++k) {
            out.push_back(seg[k]);
        }
    }
    return out;
}

std::vector<Point> cells_from_samples(const std::vector<Point>& samples) {
    std::vector<Point> cells;
    cells.reserve(samples.size());
    for (const Point& s : samples) {
        if (!finite_point(s)) {
            continue;
        }
        const Point c{std::round(s.x), std::round(s.y)};
        if (cells.empty() || cells.back() != c) {
            cells.push_back(c);
        }
    }
    return connect_cells(cells);
}

bool in_box(const Rect& box, Point p) noexcept {
    return p.x >= box.x && p.x < box.right() && p.y >= box.y && p.y < box.bottom();
}

bool path_ok(const std::vector<Point>& cells, const std::vector<Rect>& boxes,
             const WireGrid* grid) {
    if (cells.size() <= 2) {
        return true;
    }
    for (std::size_t i = 1; i + 1 < cells.size(); ++i) {
        const Point& c = cells[i];
        if (!finite_point(c) || c.x < 0.0 || c.y < 0.0) {
            return false;
        }
        for (const Rect& box : boxes) {
            if (in_box(box, c)) {
                return false;
            }
        }
        if (grid != nullptr) {
            const auto ux = static_cast<std::uint32_t>(c.x);
            const auto uy = static_cast<std::uint32_t>(c.y);
            if (!grid->in_bounds(ux, uy) || grid->occupied(ux, uy)) {
                return false;
            }
        }
    }
    return true;
}

void place_path(WireGrid& grid, const std::vector<Point>& cells) {
    for (std::size_t i = 1; i + 1 < cells.size(); ++i) {
        const Point& c = cells[i];
        if (!finite_point(c) || c.x < 0.0 || c.y < 0.0) {
            continue;
        }
        (void)grid.occupy(static_cast<std::uint32_t>(c.x), static_cast<std::uint32_t>(c.y));
    }
}

std::vector<Rect> collect_boxes(const Graph& graph) {
    std::vector<Rect> boxes;
    boxes.reserve(graph.nodes().size());
    for (const Node& node : graph.nodes()) {
        const Rect box = node.box();
        if (!(box.w >= 1.0) || !(box.h >= 1.0) || !std::isfinite(box.x) || !std::isfinite(box.y) ||
            !std::isfinite(box.w) || !std::isfinite(box.h)) {
            continue;
        }
        boxes.push_back(box);
    }
    return boxes;
}

void reserve_nodes(const Graph& graph, WireGrid& grid) {
    for (const Node& node : graph.nodes()) {
        const Rect box = node.box();
        if (!(box.w >= 1.0) || !(box.h >= 1.0) || box.x < 0.0 || box.y < 0.0 ||
            !std::isfinite(box.x) || !std::isfinite(box.y) || !std::isfinite(box.w) ||
            !std::isfinite(box.h)) {
            continue;  // 负坐标框无法登记为栅格障碍（栅格是无符号索引）
        }
        const auto x0 = static_cast<std::uint32_t>(box.x);
        const auto x1 = static_cast<std::uint32_t>(box.right() - 1.0);
        const auto y0 = static_cast<std::uint32_t>(box.y);
        const auto y1 = static_cast<std::uint32_t>(box.bottom() - 1.0);
        for (std::uint32_t y = y0; y <= y1 && y < grid.height(); ++y) {
            (void)grid.occupy_h(x0, x1, y);
        }
    }
}

void fill_local_grid(const Graph& graph, int max_detour, WireGrid& grid) {
    const Rect b = graph.bounds();
    const double margin = static_cast<double>(std::max(4, max_detour + 2));
    double w = std::isfinite(b.right()) ? std::ceil(std::max(0.0, b.right())) + margin : margin;
    double h = std::isfinite(b.bottom()) ? std::ceil(std::max(0.0, b.bottom())) + margin : margin;
    if (!std::isfinite(w) || w < 1.0) {
        w = margin;
    }
    if (!std::isfinite(h) || h < 1.0) {
        h = margin;
    }
    // 上限：坐标异常大时栅格会撑爆内存；截断只会让候选被判越界并退化，不会崩。
    w = std::min(w, 8192.0);
    h = std::min(h, 8192.0);
    grid.resize(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h));
}

// 两点之间的弧（三次贝塞尔，控制点沿弦外推 + 垂直隆起）。
std::vector<Point> arc_between(Point a, Point b, const RouteOptions& opts,
                               const std::vector<Rect>& boxes, const WireGrid* grid) {
    if (a == b) {
        return {a};
    }
    const double len = length(b - a);
    if (!(len >= 1.0)) {
        return cells_from_samples({a, b});
    }
    const Point dir = unit(b - a);
    const Point perp{-dir.y, dir.x};
    const Point mid{(a.x + b.x) / 2.0, (a.y + b.y) / 2.0};
    const double bulge0 =
        std::max(1.0, std::min(static_cast<double>(opts.max_detour), len * 0.5));
    const double factors[2] = {1.0, 0.5};
    for (double factor : factors) {
        for (int sign = 1; sign >= -1; sign -= 2) {
            const double off = bulge0 * factor * static_cast<double>(sign);
            const Point c1{a.x + dir.x * (len / 3.0) + perp.x * off,
                           a.y + dir.y * (len / 3.0) + perp.y * off};
            const Point c2{b.x - dir.x * (len / 3.0) + perp.x * off,
                           b.y - dir.y * (len / 3.0) + perp.y * off};
            std::vector<Point> curve = cells_from_samples(cubic_samples(a, c1, c2, b));
            if (!curve.empty() && path_ok(curve, boxes, grid)) {
                return curve;
            }
        }
    }
    return cells_from_samples({a, b});  // 放不下：退化为直线（宁可直连也不斜穿节点）
}

// 自环绕行：整圈都在节点框外侧。
std::vector<Point> self_loop_path(Rect box, const RouteOptions& opts,
                                  const std::vector<Rect>& boxes, const WireGrid* grid) {
    // 向外绕出的格数：自环至少要有 2 格外扩，抹角后的内侧格才会留在框外。
    const int ext = std::max(2, std::min(opts.max_detour, 3));
    const double xr = box.right() + static_cast<double>(ext);
    std::vector<Point> poly;
    if (box.h >= 3.0) {
        const Point a = snap(port_anchor(box, PortSide::East, 0.25));
        const Point b = snap(port_anchor(box, PortSide::East, 0.75));
        if (a != b) {
            poly = {a, Point{xr, a.y}, Point{xr, b.y}, b};
        }
    }
    if (poly.empty()) {
        // 矮框东侧只有一两个格，相邻两锚点会重合；改用"东侧出、南侧进"绕右下角。
        const Point a = snap(port_anchor(box, PortSide::East, 0.5));
        const Point b = snap(port_anchor(box, PortSide::South, 0.5));
        if (a == b) {
            return {a};  // 1×1 之类退化框：无外侧空间，明确退化为单点
        }
        const double yb = box.bottom() + static_cast<double>(ext);
        poly = {a, Point{xr, a.y}, Point{xr, yb}, Point{b.x, yb}, b};
    }
    const std::vector<Point> rounded = cells_from_samples(round_corners(poly, 1.0));
    if (path_ok(rounded, boxes, grid)) {
        return rounded;
    }
    const std::vector<Point> raw = cells_from_samples(poly);
    if (path_ok(raw, boxes, grid)) {
        return raw;  // 抹圆后压到别的节点：退回未抹圆的正交外形
    }
    return raw;  // 实在放不下：保留正交外形（可能压到别的节点，宁可如此也不消失）
}

}  // namespace

std::size_t route_arc(Graph& graph, const RouteOptions& opts, WireGrid* grid) {
    const std::size_t count = graph.edges().size();
    if (count == 0) {
        return 0;
    }

    // 普通边借用直线路由求锚点：RouteStyle::Straight 的 route 恰是
    // [from_anchor, to_anchor]，与其它风格共用同一套"端口分组 + 吸附整数格"逻辑。
    // 自环边不走这条（其两侧锚点会重合），改用本文件按框高度自选的锚点。
    (void)route_edges(graph, RouteStyle::Straight, opts, nullptr);

    const std::vector<Rect> boxes = collect_boxes(graph);

    WireGrid local;
    WireGrid* g = grid;
    if (g == nullptr) {
        fill_local_grid(graph, opts.max_detour, local);
        g = &local;
    }
    reserve_nodes(graph, *g);

    std::size_t routed = 0;
    for (std::size_t i = 0; i < count; ++i) {
        Edge& edge = graph.edges()[i];
        const Node* from = graph.find(edge.from());
        const Node* to = graph.find(edge.to());
        if (from == nullptr || to == nullptr) {
            continue;
        }

        std::vector<Point> chosen;
        if (edge.from() == edge.to()) {
            const Rect box = from->box();
            if (box.w >= 1.0 && box.h >= 1.0 && box.x >= 0.0 && box.y >= 0.0 &&
                std::isfinite(box.x) && std::isfinite(box.y)) {
                chosen = self_loop_path(box, opts, boxes, g);
            }
        } else {
            const std::vector<Point>& anchor_route = edge.route();
            if (anchor_route.size() < 2) {
                continue;  // 端点缺失：保持空折线
            }
            const Point a = anchor_route.front();
            const Point b = anchor_route.back();
            chosen = arc_between(a, b, opts, boxes, g);
            if (!chosen.empty()) {
                chosen.front() = a;  // 端点钉死在锚点上，抵消取整漂移
                chosen.back() = b;
            }
        }
        if (chosen.empty()) {
            continue;
        }
        place_path(*g, chosen);
        edge.set_route(std::move(chosen));
        ++routed;
    }
    return routed;
}

}  // namespace termchart::graph
