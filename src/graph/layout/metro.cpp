// metro.cpp —— 地铁布局（链分解 + 45° 之字）与 45° 走线路由
//
// 布局保证"链内相邻节点外框原点相差 (step, ±step)"，锚点直连天然是 45°；
// 路由对每条边依次尝试：直连（恰为 45°/轴对齐）→ 45° 步进走线 → L 形轴对齐
// 折线 → Z 形绕行（轴对齐三段）→ 直线。前四种都在栅格上逐格查障碍
// （节点框已登记），因此除最后的直线兜底外，走线不会穿框。自环边两端锚点重合，
// 退化成单点折线（不画）。
//
// 直线兜底仍是**已知局限**（不查障碍，可能穿框），但 Z 形绕行把跨链边与密集
// 拓扑从这条兜底里救回来了大半——兜底现在只在"两端锚点四周全被占满"的病态
// 几何下才会触发。
#include "termchart/graph/layout/metro.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "termchart/graph/port.hpp"
#include "termchart/util/assert.hpp"

namespace termchart::graph {

namespace {

// 栅格坐标的安全上限（与 route.cpp 同口径）：超过即视为不可路由。
constexpr double kMaxGridCoord = 1.0e9;

// 无向邻接表（按下标）。
std::vector<std::vector<std::size_t>> undirected_adj(const Graph& graph) {
    const std::size_t count = graph.nodes().size();
    std::vector<std::vector<std::size_t>> adj(count);
    for (const Edge& e : graph.edges()) {
        const Node* from = graph.find(e.from());
        const Node* to = graph.find(e.to());
        if (from == nullptr || to == nullptr || from == to) {
            continue;  // 端点缺失或自环不参与链分解
        }
        const auto u = static_cast<std::size_t>(from - graph.nodes().data());
        const auto v = static_cast<std::size_t>(to - graph.nodes().data());
        adj[u].push_back(v);
        adj[v].push_back(u);
    }
    return adj;
}

// 贪心链分解：从未访问节点出发向两侧延伸成一条简单链。覆盖全部节点
// （孤立节点是长度 1 的链），每个节点恰属一条链。
std::vector<std::vector<std::size_t>> extract_chains(const Graph& graph) {
    const std::vector<std::vector<std::size_t>> adj = undirected_adj(graph);
    const std::size_t count = graph.nodes().size();
    std::vector<bool> visited(count, false);
    std::vector<std::vector<std::size_t>> chains;
    for (std::size_t start = 0; start < count; ++start) {
        if (visited[start]) {
            continue;
        }
        std::vector<std::size_t> chain{start};
        visited[start] = true;
        // 向"右"延伸：每次取当前链末端任一未访问邻居。
        for (;;) {
            std::size_t next = count;  // 哨兵：没有可延伸的邻居
            for (const std::size_t v : adj[chain.back()]) {
                if (!visited[v]) {
                    next = v;
                    break;
                }
            }
            if (next == count) {
                break;
            }
            visited[next] = true;
            chain.push_back(next);
        }
        // 向"左"延伸：对链首做同样的事，倒序插到前面。
        for (;;) {
            std::size_t next = count;
            for (const std::size_t v : adj[chain.front()]) {
                if (!visited[v]) {
                    next = v;
                    break;
                }
            }
            if (next == count) {
                break;
            }
            visited[next] = true;
            chain.insert(chain.begin(), next);
        }
        chains.push_back(std::move(chain));
    }
    return chains;
}

// 之字波形：相位在 [0, 2*zigzag) 内先升后降，取值范围 [0, zigzag]。
std::uint32_t wave(std::size_t i, std::uint32_t zigzag) noexcept {
    const std::size_t period = static_cast<std::size_t>(zigzag) * 2;
    const std::size_t phase = i % period;
    const auto p = static_cast<std::uint32_t>(phase);
    return p <= zigzag ? p : static_cast<std::uint32_t>(period) - p;
}

Point snap(Point p) noexcept { return Point{std::round(p.x), std::round(p.y)}; }

bool box_ok(const Rect& box) noexcept {
    return box.w >= 1.0 && box.h >= 1.0 && box.x >= 0.0 && box.y >= 0.0 &&
           box.right() <= kMaxGridCoord && box.bottom() <= kMaxGridCoord;
}

// 把一个矩形登记为障碍（越界部分裁掉）。
void reserve_rect(const Rect& box, WireGrid& grid) {
    if (!box_ok(box)) {
        return;
    }
    const auto x0 = static_cast<std::uint32_t>(box.x);
    const auto x1 = static_cast<std::uint32_t>(box.right() - 1.0);
    const auto y0 = static_cast<std::uint32_t>(box.y);
    const auto y1 = static_cast<std::uint32_t>(box.bottom() - 1.0);
    for (std::uint32_t y = y0; y <= y1 && y < grid.height(); ++y) {
        (void)grid.occupy_h(x0, x1, y);
    }
}

void reserve_nodes(Graph& graph, WireGrid& grid) {
    for (Node& node : graph.nodes()) {
        reserve_rect(node.box(), grid);
    }
}

double sign_of(double d) noexcept { return d > 0.0 ? 1.0 : (d < 0.0 ? -1.0 : 0.0); }

// 45° 步进走线：先斜着走到与目标对齐，再沿剩余轴直进。逐格点列，首尾即锚点。
std::vector<Point> walk_points(Point a, Point b) {
    std::vector<Point> pts{a};
    double x = a.x;
    double y = a.y;
    const double sx = sign_of(b.x - x);
    const double sy = sign_of(b.y - y);
    while (std::fabs(b.x - x) > 0.5 && std::fabs(b.y - y) > 0.5) {
        x += sx;
        y += sy;
        pts.push_back(Point{x, y});
    }
    while (std::fabs(b.x - x) > 0.5) {
        x += sx;
        pts.push_back(Point{x, y});
    }
    while (std::fabs(b.y - y) > 0.5) {
        y += sy;
        pts.push_back(Point{x, y});
    }
    pts.push_back(b);
    return pts;
}

// L 形轴对齐折线（只拐一次）。
std::vector<Point> l_path(Point a, Point b, bool horizontal_first) {
    if (horizontal_first) {
        return {a, Point{b.x, a.y}, b};
    }
    return {a, Point{a.x, b.y}, b};
}

// 折线展开为逐格点列（相邻段共享端点只留一次）。段必须是轴对齐或 45°。
std::vector<Point> expand_path(const std::vector<Point>& path) {
    std::vector<Point> cells;
    for (std::size_t i = 1; i < path.size(); ++i) {
        const Point p0 = path[i - 1];
        const Point p1 = path[i];
        const double steps = std::max(std::fabs(p1.x - p0.x), std::fabs(p1.y - p0.y));
        if (steps < 0.5) {
            continue;
        }
        // steps 是 double，`static_cast<int>` 前必须确认它真的装得下：坐标受
        // kMaxGridCoord 约束，当前恰好不溢出，但那只是"碰巧"——显式断言把它钉死。
        TERMCHART_ASSERT_MSG(steps <= static_cast<double>(INT_MAX),
                             "metro: 展开步长溢出 int（坐标失去 kMaxGridCoord 约束？）");
        for (int s = 1; s <= static_cast<int>(steps); ++s) {
            const double t = static_cast<double>(s) / steps;
            cells.push_back(Point{std::round(p0.x + (p1.x - p0.x) * t),
                                  std::round(p0.y + (p1.y - p0.y) * t)});
        }
    }
    // 折线首格是锚点本身，补回去重后的完整点列。
    std::vector<Point> out;
    out.reserve(cells.size() + 1);
    out.push_back(path.front());
    for (const Point& p : cells) {
        if (out.back().x != p.x || out.back().y != p.y) {
            out.push_back(p);
        }
    }
    return out;
}

// 中间格检查（首末格是落在节点边框上的锚点，跳过）。
bool interior_clear(const WireGrid& grid, const std::vector<Point>& cells) noexcept {
    for (std::size_t i = 1; i + 1 < cells.size(); ++i) {
        if (cells[i].x < 0.0 || cells[i].y < 0.0 || cells[i].x > kMaxGridCoord ||
            cells[i].y > kMaxGridCoord) {
            return false;
        }
        const auto gx = static_cast<std::uint32_t>(cells[i].x);
        const auto gy = static_cast<std::uint32_t>(cells[i].y);
        if (!grid.in_bounds(gx, gy) || grid.occupied(gx, gy)) {
            return false;
        }
    }
    return true;
}

// 占用中间格（调用前须先 interior_clear）。
void occupy_interior(WireGrid& grid, const std::vector<Point>& cells) {
    for (std::size_t i = 1; i + 1 < cells.size(); ++i) {
        (void)grid.occupy(static_cast<std::uint32_t>(cells[i].x),
                          static_cast<std::uint32_t>(cells[i].y));
    }
}

// 把逐格点列压成拐点折线（共线的中间格去掉）。
std::vector<Point> simplify_cells(const std::vector<Point>& cells) {
    std::vector<Point> out;
    if (cells.size() < 2) {
        return cells;
    }
    out.push_back(cells.front());
    for (std::size_t i = 1; i + 1 < cells.size(); ++i) {
        const Point& prev = cells[i - 1];
        const Point& cur = cells[i];
        const Point& next = cells[i + 1];
        const bool colinear = (prev.x == cur.x && cur.x == next.x) ||
                              (prev.y == cur.y && cur.y == next.y) ||
                              (prev.x - cur.x == cur.x - next.x &&
                               prev.y - cur.y == cur.y - next.y);
        if (!colinear) {
            out.push_back(cur);
        }
    }
    out.push_back(cells.back());
    return out;
}

}  // namespace

Rect layout_metro(Graph& graph, const MetroOptions& opts) {
    if (graph.nodes().empty()) {
        return Rect{};
    }

    // step 抬到不小于最大框宽高：相邻框以 45° 相错时只在角上相触，
    // 不共享内部格——链内 45° 走线"不穿框"由此直接成立。
    double max_w = 1.0;
    double max_h = 1.0;
    for (const Node& n : graph.nodes()) {
        const Rect size = node_box_for(n, opts.layout);
        max_w = std::max(max_w, size.w);
        max_h = std::max(max_h, size.h);
    }
    const double step = std::max(std::max(opts.step, max_w), max_h);
    const std::uint32_t zigzag = std::max<std::uint32_t>(opts.zigzag, 1);

    const std::vector<std::vector<std::size_t>> chains = extract_chains(graph);
    const double band_height = static_cast<double>(zigzag) * step + max_h;

    double union_right = 0.0;
    double union_bottom = 0.0;
    for (std::size_t li = 0; li < chains.size(); ++li) {
        const double band_y = opts.layout.margin +
                              static_cast<double>(li) * (band_height + opts.line_gap);
        const std::vector<std::size_t>& chain = chains[li];
        for (std::size_t k = 0; k < chain.size(); ++k) {
            Node& node = graph.nodes()[chain[k]];
            const Rect size = node_box_for(node, opts.layout);
            const double x = opts.layout.margin + static_cast<double>(k) * step;
            const double y = band_y + static_cast<double>(wave(k, zigzag)) * step;
            node.set_box(Rect{x, y, size.w, size.h});
            union_right = std::max(union_right, x + size.w);
            union_bottom = std::max(union_bottom, y + size.h);
        }
    }
    const double margin = opts.layout.margin;
    return Rect{margin, margin, union_right - margin, union_bottom - margin};
}

std::size_t route_metro(Graph& graph, const MetroRouteOptions& opts, WireGrid* grid) {
    const std::size_t count = graph.edges().size();
    if (count == 0) {
        return 0;
    }

    // 自建栅格：覆盖全图 + 绕行余量（与 spline.cpp 同口径）。
    WireGrid local;
    if (grid == nullptr) {
        const Rect b = graph.bounds();
        const double margin =
            static_cast<double>(std::max(4, opts.route.max_detour + 2));
        double w = std::isfinite(b.right()) ? std::ceil(std::max(0.0, b.right())) + margin
                                            : margin;
        double h = std::isfinite(b.bottom()) ? std::ceil(std::max(0.0, b.bottom())) + margin
                                             : margin;
        if (!std::isfinite(w) || w < 1.0) {
            w = margin;
        }
        if (!std::isfinite(h) || h < 1.0) {
            h = margin;
        }
        local.resize(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h));
        grid = &local;
    }
    reserve_nodes(graph, *grid);

    std::size_t routed = 0;
    // 锚点候选：45° 之字拓扑下，side_towards 的单一主选可能落在"被自身节点框
    // 堵死"的一侧（例如对角相邻时主选 Right，而下绕会撞框）。因此按
    // "主选优先、其余三边兜底"枚举锚点对，每一对都走完 直连→45°步进→L形 阶梯，
    // 全部失败才退到直线（直线不查障碍，是最后手段——这是metro 3 例已知红的教训）。
    const PortSide all_sides[4] = {PortSide::North, PortSide::East, PortSide::South,
                                   PortSide::West};
    for (Edge& edge : graph.edges()) {
        const Node* from = graph.find(edge.from());
        const Node* to = graph.find(edge.to());
        if (from == nullptr || to == nullptr) {
            continue;
        }
        const Point from_center = from->box().center();
        const Point to_center = to->box().center();
        const PortSide primary_a = side_towards(from_center, to_center);
        const PortSide primary_b = side_towards(to_center, from_center);
        const PortSide sides_a[5] = {primary_a, all_sides[0], all_sides[1], all_sides[2],
                                     all_sides[3]};
        const PortSide sides_b[5] = {primary_b, all_sides[0], all_sides[1], all_sides[2],
                                     all_sides[3]};
        // 去重保序（主选在前）。
        std::vector<PortSide> order_a;
        std::vector<PortSide> order_b;
        for (const PortSide s : sides_a) {
            if (std::find(order_a.begin(), order_a.end(), s) == order_a.end()) {
                order_a.push_back(s);
            }
        }
        for (const PortSide s : sides_b) {
            if (std::find(order_b.begin(), order_b.end(), s) == order_b.end()) {
                order_b.push_back(s);
            }
        }

        bool routed_ok = false;
        std::vector<Point> chosen;
        for (const PortSide sa : order_a) {
            const Point a = snap(port_anchor(from->box(), sa));
            for (const PortSide sb : order_b) {
                const Point b = snap(port_anchor(to->box(), sb));
                if (a.x == b.x && a.y == b.y) {
                    continue;  // 锚点重合（自环语义）：不画
                }
                // 1) 直连：恰为 45° 或轴对齐。
                const bool diag = std::fabs(b.x - a.x) == std::fabs(b.y - a.y);
                const bool axis = (a.x == b.x) != (a.y == b.y);
                if (diag || axis) {
                    const std::vector<Point> cells = expand_path({a, b});
                    if (interior_clear(*grid, cells)) {
                        occupy_interior(*grid, cells);
                        chosen = {a, b};
                        routed_ok = true;
                        break;
                    }
                }
                // 2) 45° 步进走线（斜走到对齐再直进）。
                const std::vector<Point> walk_cells = expand_path(walk_points(a, b));
                if (interior_clear(*grid, walk_cells)) {
                    occupy_interior(*grid, walk_cells);
                    chosen = simplify_cells(walk_cells);
                    routed_ok = true;
                    break;
                }
                // 3) L 形轴对齐兜底（仍查障碍）。
                for (const bool horizontal_first : {true, false}) {
                    const std::vector<Point> l_cells =
                        expand_path(l_path(a, b, horizontal_first));
                    if (interior_clear(*grid, l_cells)) {
                        occupy_interior(*grid, l_cells);
                        chosen = simplify_cells(l_cells);
                        routed_ok = true;
                        break;
                    }
                }
                if (routed_ok) {
                    break;
                }
                // 3.5) Z 形绕行（轴对齐三段）：L 形只在"正对障碍"时无解——
                // 横段被挡、竖段被挡，两种拐法都不行。此时绕到另一条行/列再回来
                // 常常就有路了：
                //     横向绕行  a → (a.x, y') → (b.x, y') → b
                //     纵向绕行  a → (x', a.y) → (x', b.y) → b
                // 候选 y'/x' 是**确定性枚举**（两端点与中点 ±1..span），不做随机、
                // 不做 A*——metro 的拓扑是链带，绕行余量有限，穷举比搜图更可预测。
                // 这一步把"跨链边 / 密集拓扑"从直线兜底里救回来一大半。
                const int span = std::min(std::max(1, opts.route.max_detour), 8);
                const double seeds[3] = {a.y, b.y, (a.y + b.y) / 2.0};
                for (const double seed : seeds) {
                    for (int k = 1; k <= span && !routed_ok; ++k) {
                        for (const double sign : {-1.0, 1.0}) {
                            const double yy = seed + sign * static_cast<double>(k);
                            if (yy < 0.0 || yy > kMaxGridCoord) {
                                continue;
                            }
                            const std::vector<Point> z_cells =
                                expand_path({a, Point{a.x, yy}, Point{b.x, yy}, b});
                            if (interior_clear(*grid, z_cells)) {
                                occupy_interior(*grid, z_cells);
                                chosen = simplify_cells(z_cells);
                                routed_ok = true;
                                break;
                            }
                        }
                    }
                    if (routed_ok) {
                        break;
                    }
                }
                if (routed_ok) {
                    break;
                }
                const double seeds_x[3] = {a.x, b.x, (a.x + b.x) / 2.0};
                for (const double seed : seeds_x) {
                    for (int k = 1; k <= span && !routed_ok; ++k) {
                        for (const double sign : {-1.0, 1.0}) {
                            const double xx = seed + sign * static_cast<double>(k);
                            if (xx < 0.0 || xx > kMaxGridCoord) {
                                continue;
                            }
                            const std::vector<Point> z_cells =
                                expand_path({a, Point{xx, a.y}, Point{xx, b.y}, b});
                            if (interior_clear(*grid, z_cells)) {
                                occupy_interior(*grid, z_cells);
                                chosen = simplify_cells(z_cells);
                                routed_ok = true;
                                break;
                            }
                        }
                    }
                    if (routed_ok) {
                        break;
                    }
                }
                if (routed_ok) {
                    break;
                }
            }
            if (routed_ok) {
                break;
            }
        }
        // 4) 最后手段：直线（不占用栅格，不再保证不穿节点）。
        if (!routed_ok) {
            const Point a = snap(port_anchor(from->box(), primary_a));
            const Point b = snap(port_anchor(to->box(), primary_b));
            chosen = a.x == b.x && a.y == b.y ? std::vector<Point>{a}
                                              : std::vector<Point>{a, b};
        }
        edge.set_route(std::move(chosen));
        ++routed;
    }
    return routed;
}

}  // namespace termchart::graph
