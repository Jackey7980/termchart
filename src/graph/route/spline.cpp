// spline.cpp —— 平滑曲线路由：正交途经点做控制多边形 + Catmull-Rom 采样
//
// 为什么复用正交途经点当控制多边形：曲线本身没有"该往哪绕"的语义，而正交路由的
// Z 形 / L 形已经把"从哪一侧出、在哪个空档处转弯"编码成了折点。以它为控制点，曲线
// 只是把折角抹圆，既保留了避让意图，又天然会在节点框之间穿过。
//
// 为什么按弦长自适应采样：固定步数会让长段稀疏（台阶明显）、短段浪费（大量重复格）。
// 每格 4 个采样点足够让取整后的格序列连续，又不会产生无意义的中间点。
#include "termchart/graph/route/spline.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/canvas/primitives.hpp"
#include "termchart/graph/route/route.hpp"

namespace termchart::graph {

namespace {

// 栅格坐标的安全上限：超过即视为非法，避免窄化转换越界。
constexpr double kMaxGridCoord = 1.0e9;

// 每格弦长的采样点数（只影响平滑度，不影响端点）。
constexpr double kSamplesPerCell = 4.0;

bool finite_point(Point p) noexcept {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::fabs(p.x) <= kMaxGridCoord &&
           std::fabs(p.y) <= kMaxGridCoord;
}

std::size_t sample_steps(double span) noexcept {
    if (!(span > 0.0)) {
        return 2;  // 退化段也至少两个采样点，避免 0/0
    }
    const double raw = std::ceil(span * kSamplesPerCell) + 2.0;
    if (raw >= 8192.0) {
        return 8192;  // 上限：异常大的坐标不该让采样爆掉
    }
    return static_cast<std::size_t>(raw);
}

void dedupe_consecutive(std::vector<Point>& pts) {
    std::vector<Point> compact;
    compact.reserve(pts.size());
    for (const Point& p : pts) {
        if (compact.empty() || compact.back() != p) {
            compact.push_back(p);
        }
    }
    pts.swap(compact);
}

// 正交控制多边形：横向出/竖向绕/横向进时 mid 是 x，否则 mid 是 y。
// 与 route.cpp 的 orthogonal_path 同形；这里不化简共线点，交给 dedupe_consecutive。
std::vector<Point> control_polygon(Point a, Point b, bool horizontal, double mid) {
    std::vector<Point> pts;
    if (horizontal) {
        pts = {a, Point{mid, a.y}, Point{mid, b.y}, b};
    } else {
        pts = {a, Point{a.x, mid}, Point{b.x, mid}, b};
    }
    dedupe_consecutive(pts);
    return pts;
}

// Catmull-Rom 在 P1→P2 段上的取值（P0/P3 只提供切线）。
Point catmull_rom(Point p0, Point p1, Point p2, Point p3, double t) noexcept {
    const double t2 = t * t;
    const double t3 = t2 * t;
    const auto axis = [t, t2, t3](double v0, double v1, double v2, double v3) {
        return 0.5 * ((2.0 * v1) + (-v0 + v2) * t + (2.0 * v0 - 5.0 * v1 + 4.0 * v2 - v3) * t2 +
                      (-v0 + 3.0 * v1 - 3.0 * v2 + v3) * t3);
    };
    return Point{axis(p0.x, p1.x, p2.x, p3.x), axis(p0.y, p1.y, p2.y, p3.y)};
}

// 端点切线用"复制端点"求（等价于让首末段的切线取相邻弦方向），比额外外推两个
// 幽灵控制点更稳：不会把曲线甩出节点框。
std::vector<Point> catmull_rom_samples(const std::vector<Point>& c) {
    std::vector<Point> out;
    const std::size_t n = c.size();
    if (n == 0) {
        return out;
    }
    if (n == 1) {
        out.push_back(c[0]);
        return out;
    }
    if (n == 2) {
        out.push_back(c[0]);
        out.push_back(c[1]);
        return out;
    }
    out.reserve(n * 8);
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const Point p0 = (i == 0) ? c[0] : c[i - 1];
        const Point p1 = c[i];
        const Point p2 = c[i + 1];
        const Point p3 = (i + 2 < n) ? c[i + 2] : c[i + 1];
        const std::size_t steps = sample_steps(std::hypot(p2.x - p1.x, p2.y - p1.y));
        for (std::size_t k = 0; k < steps; ++k) {
            out.push_back(catmull_rom(p0, p1, p2, p3,
                                      static_cast<double>(k) / static_cast<double>(steps)));
        }
    }
    out.push_back(c.back());
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

// 浮点采样 → 逐格取整 → 去重 → 补 8 连通步。非法采样点直接丢弃。
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

// 路径可用性：首末格是落在节点边框上的锚点（节点框自身也是障碍），跳过；其余格
// 既不能落进任何节点框，也不能压到已占用的格。
bool path_ok(const std::vector<Point>& cells, const std::vector<Rect>& boxes,
             const WireGrid* grid) {
    if (cells.size() <= 2) {
        return true;  // 只有两个锚点格：无中间格可撞
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

// 把节点外框登记为障碍，避免曲线穿过节点。
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

// grid 为空时自建栅格：覆盖全图并留出绕行余量。
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
    // 上限：坐标异常大时栅格会撑爆内存；截断只会让绕行候选被判越界并退化，不会崩。
    w = std::min(w, 8192.0);
    h = std::min(h, 8192.0);
    grid.resize(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h));
}

}  // namespace

std::size_t route_spline(Graph& graph, const RouteOptions& opts, WireGrid* grid) {
    const std::size_t count = graph.edges().size();
    if (count == 0) {
        return 0;
    }

    // 借用直线路由求锚点：RouteStyle::Straight 写入的 route 恰是
    // [from_anchor, to_anchor]，与其它风格共用同一套"端口分组 + 吸附整数格"逻辑。
    // 在本文件重写一份端口分配只会让两条路由慢慢分叉。
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
        edge.set_route_failed(false);  // 重置：本次重算的成败以此为准（雷区1 信号）
        const std::vector<Point>& anchor_route = edge.route();
        if (anchor_route.size() < 2) {
            continue;  // 端点缺失：保持空折线
        }
        const Point a = anchor_route.front();
        const Point b = anchor_route.back();

        const Node* from = graph.find(edge.from());
        const Node* to = graph.find(edge.to());
        if (from == nullptr || to == nullptr) {
            continue;
        }

        std::vector<Point> chosen;
        std::vector<Point> fallback;
        if (a != b) {
            const Rect from_box = from->box();
            const Rect to_box = to->box();
            // 方向按**两框相对位置**判定（同 route.cpp）：按锚点距离选会让绕行坐标
            // 落到源框的边框列上，被障碍判定否决后只能退化成斜线。
            const bool x_disjoint =
                from_box.right() <= to_box.x || to_box.right() <= from_box.x;
            const bool y_disjoint =
                from_box.bottom() <= to_box.y || to_box.bottom() <= from_box.y;
            bool horizontal = std::fabs(b.x - a.x) >= std::fabs(b.y - a.y);
            if (x_disjoint) {
                horizontal = true;
            } else if (y_disjoint) {
                horizontal = false;
            }
            const double base = horizontal ? (a.x + b.x) / 2.0 : (a.y + b.y) / 2.0;
            for (int detour = 0; detour <= opts.max_detour && chosen.empty(); ++detour) {
                for (int sign = 1; sign >= -1; sign -= 2) {
                    if (detour == 0 && sign < 0) {
                        continue;  // 中点只试一次
                    }
                    const double mid = std::round(base + static_cast<double>(detour * sign));
                    const std::vector<Point> control = control_polygon(a, b, horizontal, mid);
                    const std::vector<Point> orthogonal = cells_from_samples(control);
                    if (fallback.empty() && path_ok(orthogonal, boxes, g)) {
                        fallback = orthogonal;
                    }
                    std::vector<Point> curve = cells_from_samples(catmull_rom_samples(control));
                    if (!curve.empty() && path_ok(curve, boxes, g)) {
                        chosen = std::move(curve);
                        break;
                    }
                }
            }
        }
        if (chosen.empty()) {
            chosen = std::move(fallback);  // 宁可折线也不穿框
        }
        if (chosen.empty()) {
            // 最后手段：直线（不再保证不穿框）。平滑/正交都失败 → 置 route_failed
            // 让上层提示"此处过挤"（雷区1）。
            chosen = cells_from_samples({a, b});
            edge.set_route_failed(true);
        }
        if (!chosen.empty()) {
            chosen.front() = a;  // 端点钉死在锚点上，抵消取整漂移
            chosen.back() = b;
        }
        place_path(*g, chosen);
        edge.set_route(std::move(chosen));
        ++routed;
    }
    return routed;
}

}  // namespace termchart::graph
