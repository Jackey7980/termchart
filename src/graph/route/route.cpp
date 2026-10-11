// route.cpp —— 直线与正交路由实现
//
// 端口分配：同一节点（或同一入侧）上可能挂多条边。若都取侧边中点，后路由的边
// 必然与先路由的边在首格冲突，只能退化成斜直线。因此先按 (节点, 侧) 分组，
// 组内第 k 条边取 t = (k+1)/(n+1)，让多条边沿侧边均匀扇出。
#include "termchart/graph/route/route.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "termchart/graph/port.hpp"

namespace termchart::graph {

namespace {

// 栅格坐标的安全上限：超过即视为不可路由，避免窄化转换越界。
constexpr double kMaxGridCoord = 1.0e9;

using PortGroupKey = std::pair<NodeId, int>;

int side_key(PortSide side) noexcept { return static_cast<int>(side); }

// 锚点吸附到整数格：字符网格上不存在半格，锚点带小数会让 Z 形出现 0.5 格抖动，
// 也会让折线坐标与栅格坐标对不齐。
Point snap_to_cell(Point p) noexcept { return Point{std::round(p.x), std::round(p.y)}; }

// 组内第 index 条边的归一化位置：n 条边分 n+1 段，取各段端点。
double fraction_in(const std::vector<std::size_t>& group, std::size_t index) noexcept {
    for (std::size_t k = 0; k < group.size(); ++k) {
        if (group[k] == index) {
            return static_cast<double>(k + 1) / static_cast<double>(group.size() + 1);
        }
    }
    return 0.5;
}

// 去掉连续重复点与共线的中间点。
std::vector<Point> simplify(std::vector<Point> points) {
    std::vector<Point> compact;
    compact.reserve(points.size());
    for (const Point& p : points) {
        if (compact.empty() || compact.back().x != p.x || compact.back().y != p.y) {
            compact.push_back(p);
        }
    }
    std::vector<Point> result;
    result.reserve(compact.size());
    for (std::size_t i = 0; i < compact.size(); ++i) {
        if (i > 0 && i + 1 < compact.size()) {
            const bool same_x =
                compact[i - 1].x == compact[i].x && compact[i].x == compact[i + 1].x;
            const bool same_y =
                compact[i - 1].y == compact[i].y && compact[i].y == compact[i + 1].y;
            if (same_x || same_y) {
                continue;  // 共线中间点
            }
        }
        result.push_back(compact[i]);
    }
    return result;
}

// 轴对齐的 Z 形折线。方向由调用方决定（按节点框相对位置），不能在这里按锚点
// 距离重新判断——否则会覆盖调用方的选择，甚至让绕行坐标越出画布。
// horizontal == true 时 mid 是 x（横向出、竖向绕行、横向进），否则 mid 是 y。
std::vector<Point> orthogonal_path(Point a, Point b, bool horizontal, double mid) {
    if (horizontal) {
        return simplify({a, Point{mid, a.y}, Point{mid, b.y}, b});
    }
    return simplify({a, Point{a.x, mid}, Point{b.x, mid}, b});
}

bool segment_to_grid(Point p0, Point p1, std::uint32_t& x0, std::uint32_t& y0,
                     std::uint32_t& x1, std::uint32_t& y1) {
    if (p0.x < 0.0 || p0.y < 0.0 || p1.x < 0.0 || p1.y < 0.0) {
        return false;
    }
    if (p0.x > kMaxGridCoord || p0.y > kMaxGridCoord || p1.x > kMaxGridCoord ||
        p1.y > kMaxGridCoord) {
        return false;
    }
    x0 = static_cast<std::uint32_t>(p0.x);
    y0 = static_cast<std::uint32_t>(p0.y);
    x1 = static_cast<std::uint32_t>(p1.x);
    y1 = static_cast<std::uint32_t>(p1.y);
    return true;
}

using Cell = std::pair<std::uint32_t, std::uint32_t>;

// 把折线展平成栅格格序列（相邻段共享的端点只保留一次）。
// 任一线段非轴对齐或坐标非法时返回 false。
bool path_cells(const std::vector<Point>& path, std::vector<Cell>& out) {
    out.clear();
    for (std::size_t i = 1; i < path.size(); ++i) {
        std::uint32_t x0 = 0;
        std::uint32_t y0 = 0;
        std::uint32_t x1 = 0;
        std::uint32_t y1 = 0;
        if (!segment_to_grid(path[i - 1], path[i], x0, y0, x1, y1)) {
            return false;
        }
        if (y0 == y1) {
            const std::uint32_t lo = std::min(x0, x1);
            const std::uint32_t hi = std::max(x0, x1);
            for (std::uint32_t x = lo; x <= hi; ++x) {
                out.emplace_back(x, y0);
            }
        } else if (x0 == x1) {
            const std::uint32_t lo = std::min(y0, y1);
            const std::uint32_t hi = std::max(y0, y1);
            for (std::uint32_t y = lo; y <= hi; ++y) {
                out.emplace_back(x0, y);
            }
        } else {
            return false;  // 斜线段不由正交路由产生
        }
    }
    std::vector<Cell> unique;
    unique.reserve(out.size());
    for (const Cell& cell : out) {
        if (unique.empty() || unique.back() != cell) {
            unique.push_back(cell);
        }
    }
    out.swap(unique);
    return true;
}

// 探测可用（不占用）。首格与末格是落在节点边框上的锚点（节点框已登记为障碍），
// 故跳过它们，只检查中间格。
bool can_place(const WireGrid& grid, const std::vector<Point>& path) {
    std::vector<Cell> cells;
    if (!path_cells(path, cells)) {
        return false;
    }
    if (cells.size() <= 2) {
        return true;  // 只有两个锚点格：无中间格可撞
    }
    for (std::size_t i = 1; i + 1 < cells.size(); ++i) {
        if (!grid.in_bounds(cells[i].first, cells[i].second) ||
            grid.occupied(cells[i].first, cells[i].second)) {
            return false;
        }
    }
    return true;
}

// 占用整条折线（调用前须先 can_place）。
void place(WireGrid& grid, const std::vector<Point>& path) {
    std::vector<Cell> cells;
    if (!path_cells(path, cells)) {
        return;
    }
    for (std::size_t i = 1; i + 1 < cells.size(); ++i) {
        (void)grid.occupy(cells[i].first, cells[i].second);
    }
}

// 把节点外框登记为障碍，避免折线穿过节点。
void reserve_nodes(Graph& graph, WireGrid& grid) {
    for (const Node& node : graph.nodes()) {
        const Rect box = node.box();
        if (box.w < 1.0 || box.h < 1.0 || box.x < 0.0 || box.y < 0.0) {
            continue;
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

}  // namespace

std::size_t route_edges(Graph& graph, RouteStyle style, const RouteOptions& opts,
                        WireGrid* grid) {
    const std::size_t count = graph.edges().size();
    if (count == 0) {
        return 0;
    }

    // 第一遍：定出每条边离开/进入的侧，并按 (节点, 侧) 分组。
    std::vector<PortSide> leave_side(count, PortSide::East);
    std::vector<PortSide> enter_side(count, PortSide::West);
    std::map<PortGroupKey, std::vector<std::size_t>> leave_groups;
    std::map<PortGroupKey, std::vector<std::size_t>> enter_groups;
    for (std::size_t i = 0; i < count; ++i) {
        const Edge& edge = graph.edges()[i];
        const Node* from = graph.find(edge.from());
        const Node* to = graph.find(edge.to());
        if (from == nullptr || to == nullptr) {
            continue;
        }
        const Point from_center{from->box().x + from->box().w / 2.0,
                                from->box().y + from->box().h / 2.0};
        const Point to_center{to->box().x + to->box().w / 2.0,
                              to->box().y + to->box().h / 2.0};
        leave_side[i] = side_towards(from_center, to_center);
        enter_side[i] = side_towards(to_center, from_center);
        leave_groups[{edge.from(), side_key(leave_side[i])}].push_back(i);
        enter_groups[{edge.to(), side_key(enter_side[i])}].push_back(i);
    }

    // 第二遍：按组内位置算锚点，使同侧多条边均匀扇出。
    std::vector<Point> from_anchor(count);
    std::vector<Point> to_anchor(count);
    std::vector<bool> routable(count, false);
    for (std::size_t i = 0; i < count; ++i) {
        const Edge& edge = graph.edges()[i];
        const Node* from = graph.find(edge.from());
        const Node* to = graph.find(edge.to());
        if (from == nullptr || to == nullptr) {
            continue;
        }
        const auto leave_it = leave_groups.find({edge.from(), side_key(leave_side[i])});
        const auto enter_it = enter_groups.find({edge.to(), side_key(enter_side[i])});
        const double leave_t =
            leave_it == leave_groups.end() ? 0.5 : fraction_in(leave_it->second, i);
        const double enter_t =
            enter_it == enter_groups.end() ? 0.5 : fraction_in(enter_it->second, i);
        from_anchor[i] = snap_to_cell(port_anchor(from->box(), leave_side[i], leave_t));
        to_anchor[i] = snap_to_cell(port_anchor(to->box(), enter_side[i], enter_t));
        routable[i] = true;
    }

    // 第三遍：先把节点框登记为障碍，再逐边定折线。
    if (grid != nullptr) {
        reserve_nodes(graph, *grid);
    }
    std::size_t routed = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (!routable[i]) {
            continue;
        }
        Edge& edge = graph.edges()[i];
        const Node* from = graph.find(edge.from());
        const Node* to = graph.find(edge.to());
        if (from == nullptr || to == nullptr) {
            continue;
        }
        const Point a = from_anchor[i];
        const Point b = to_anchor[i];

        if (style == RouteStyle::Straight || grid == nullptr) {
            edge.set_route({a, b});
            ++routed;
            continue;
        }

        // 方向按**两个节点框的相对位置**判定，而不是锚点距离：
        // 并排（x 不相交）时若按距离可能选到竖向 Z，其竖段正好压在源框边框列上，
        // 该列下方的格全在框内，会被障碍判定否决，最终退化成斜线。
        const Rect from_box = from->box();
        const Rect to_box = to->box();
        const bool x_disjoint = from_box.right() <= to_box.x || to_box.right() <= from_box.x;
        const bool y_disjoint = from_box.bottom() <= to_box.y || to_box.bottom() <= from_box.y;
        bool horizontal = std::fabs(b.x - a.x) >= std::fabs(b.y - a.y);
        if (x_disjoint) {
            horizontal = true;   // 左右分离：横向出、竖向绕行、横向进
        } else if (y_disjoint) {
            horizontal = false;  // 上下分离：竖向出、横向绕行、竖向进
        }
        const double base = horizontal ? (a.x + b.x) / 2.0 : (a.y + b.y) / 2.0;
        std::vector<Point> chosen;
        for (int detour = 0; detour <= opts.max_detour && chosen.empty(); ++detour) {
            for (int sign = 1; sign >= -1; sign -= 2) {
                if (detour == 0 && sign < 0) {
                    continue;  // 中点只试一次
                }
                const double mid = std::round(base + static_cast<double>(detour * sign));
                std::vector<Point> candidate = orthogonal_path(a, b, horizontal, mid);
                if (can_place(*grid, candidate)) {
                    place(*grid, candidate);
                    chosen = std::move(candidate);
                    break;
                }
            }
        }
        if (chosen.empty()) {
            // Z 形都不通时退一档：两种 L 形（只拐一次），仍做障碍检查。
            const std::vector<Point> horizontal_first = simplify({a, Point{b.x, a.y}, b});
            const std::vector<Point> vertical_first = simplify({a, Point{a.x, b.y}, b});
            if (can_place(*grid, horizontal_first)) {
                place(*grid, horizontal_first);
                chosen = horizontal_first;
            } else if (can_place(*grid, vertical_first)) {
                place(*grid, vertical_first);
                chosen = vertical_first;
            }
        }
        if (chosen.empty()) {
            chosen = {a, b};  // 最后手段：直线（不占用栅格，也不再保证不穿节点）
        }
        edge.set_route(std::move(chosen));
        ++routed;
    }
    return routed;
}

}  // namespace termchart::graph
