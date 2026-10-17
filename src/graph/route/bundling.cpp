// bundling.cpp —— 边捆绑实现（按目标分组 + 公共并束点 + 共享末段）
#include "termchart/graph/route/bundling.hpp"

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

constexpr double kMaxGridCoord = 1.0e9;

using Cell = std::pair<std::uint32_t, std::uint32_t>;
using PortGroupKey = std::pair<NodeId, int>;

int side_key(PortSide side) noexcept { return static_cast<int>(side); }

Point snap(Point p) noexcept { return Point{std::round(p.x), std::round(p.y)}; }

bool box_ok(const Rect& box) noexcept {
    return box.w >= 1.0 && box.h >= 1.0 && box.x >= 0.0 && box.y >= 0.0 &&
           box.right() <= kMaxGridCoord && box.bottom() <= kMaxGridCoord;
}

void reserve_nodes(Graph& graph, WireGrid& grid) {
    for (const Node& node : graph.nodes()) {
        const Rect box = node.box();
        if (!box_ok(box)) {
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

// 组内第 index 条边的归一化锚位：n 条边分 n+1 段（与 route.cpp 同口径）。
double fraction_in(const std::vector<std::size_t>& group, std::size_t index) noexcept {
    for (std::size_t k = 0; k < group.size(); ++k) {
        if (group[k] == index) {
            return static_cast<double>(k + 1) / static_cast<double>(group.size() + 1);
        }
    }
    return 0.5;
}

// 去掉连续重复点与共线中间点（与 route.cpp 同口径）。
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
                continue;
            }
        }
        result.push_back(compact[i]);
    }
    return result;
}

// 轴对齐 Z 形（horizontal == true 时 mid 是 x，否则是 y）。
std::vector<Point> orthogonal_path(Point a, Point b, bool horizontal, double mid) {
    if (horizontal) {
        return simplify({a, Point{mid, a.y}, Point{mid, b.y}, b});
    }
    return simplify({a, Point{a.x, mid}, Point{b.x, mid}, b});
}

// 轴对齐折线展开为格序列；非轴对齐或坐标非法时返回 false。
bool path_cells(const std::vector<Point>& path, std::vector<Cell>& out) {
    out.clear();
    for (std::size_t i = 1; i < path.size(); ++i) {
        const Point& p0 = path[i - 1];
        const Point& p1 = path[i];
        if (p0.x < 0.0 || p0.y < 0.0 || p1.x < 0.0 || p1.y < 0.0 ||
            p0.x > kMaxGridCoord || p0.y > kMaxGridCoord || p1.x > kMaxGridCoord ||
            p1.y > kMaxGridCoord) {
            return false;
        }
        const auto x0 = static_cast<std::uint32_t>(p0.x);
        const auto y0 = static_cast<std::uint32_t>(p0.y);
        const auto x1 = static_cast<std::uint32_t>(p1.x);
        const auto y1 = static_cast<std::uint32_t>(p1.y);
        if (y0 == y1) {
            for (std::uint32_t x = std::min(x0, x1); x <= std::max(x0, x1); ++x) {
                out.emplace_back(x, y0);
            }
        } else if (x0 == x1) {
            for (std::uint32_t y = std::min(y0, y1); y <= std::max(y0, y1); ++y) {
                out.emplace_back(x0, y);
            }
        } else {
            return false;
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

// 中间格检查（首末格是落在节点边框上的锚点，跳过）。
bool can_place(const WireGrid& grid, const std::vector<Point>& path) {
    std::vector<Cell> cells;
    if (!path_cells(path, cells)) {
        return false;
    }
    if (cells.size() <= 2) {
        return true;
    }
    for (std::size_t i = 1; i + 1 < cells.size(); ++i) {
        if (!grid.in_bounds(cells[i].first, cells[i].second) ||
            grid.occupied(cells[i].first, cells[i].second)) {
            return false;
        }
    }
    return true;
}

// 占用整条折线的中间格。
void place(WireGrid& grid, const std::vector<Point>& path) {
    std::vector<Cell> cells;
    if (!path_cells(path, cells)) {
        return;
    }
    for (std::size_t i = 1; i + 1 < cells.size(); ++i) {
        (void)grid.occupy(cells[i].first, cells[i].second);
    }
}

// 普通正交路由（单边、无捆绑）：Z 形候选逐级绕行，L 形兜底，最后直线。
// 与 route.cpp 的判定一致：方向按两个节点框的相对位置，不按锚点距离。
std::vector<Point> route_one(WireGrid& grid, Point a, Point b, const Rect& from_box,
                             const Rect& to_box, int max_detour) {
    const bool x_disjoint = from_box.right() <= to_box.x || to_box.right() <= from_box.x;
    const bool y_disjoint = from_box.bottom() <= to_box.y || to_box.bottom() <= from_box.y;
    bool horizontal = std::fabs(b.x - a.x) >= std::fabs(b.y - a.y);
    if (x_disjoint) {
        horizontal = true;
    } else if (y_disjoint) {
        horizontal = false;
    }
    const double base = horizontal ? (a.x + b.x) / 2.0 : (a.y + b.y) / 2.0;
    for (int detour = 0; detour <= max_detour; ++detour) {
        for (int sign = 1; sign >= -1; sign -= 2) {
            if (detour == 0 && sign < 0) {
                continue;
            }
            const double mid = std::round(base + static_cast<double>(detour * sign));
            std::vector<Point> candidate = orthogonal_path(a, b, horizontal, mid);
            if (can_place(grid, candidate)) {
                place(grid, candidate);
                return candidate;
            }
        }
    }
    const std::vector<Point> h_first = simplify({a, Point{b.x, a.y}, b});
    if (can_place(grid, h_first)) {
        place(grid, h_first);
        return h_first;
    }
    const std::vector<Point> v_first = simplify({a, Point{a.x, b.y}, b});
    if (can_place(grid, v_first)) {
        place(grid, v_first);
        return v_first;
    }
    return {a, b};
}

Point outward_of(PortSide side) noexcept {
    switch (side) {
        case PortSide::West:
            return Point{-1.0, 0.0};
        case PortSide::East:
            return Point{1.0, 0.0};
        case PortSide::North:
            return Point{0.0, -1.0};
        case PortSide::South:
            return Point{0.0, 1.0};
    }
    return Point{0.0, 0.0};
}

}  // namespace

std::size_t route_bundled(Graph& graph, const BundlingOptions& opts, WireGrid* grid_in) {
    const std::size_t count = graph.edges().size();
    if (count == 0) {
        return 0;
    }

    // 自建栅格：覆盖全图 + 绕行余量（与 spline.cpp 同口径）。
    WireGrid local;
    WireGrid* grid = grid_in;
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

    // 锚点：源端按 (节点, 侧) 分组扇出；目标端同理（单边组用，捆绑组被公共锚点覆盖）。
    std::vector<Point> from_anchor(count);
    std::vector<Point> to_anchor(count);
    std::vector<PortSide> leave_side(count, PortSide::East);
    std::vector<PortSide> enter_side(count, PortSide::West);
    std::vector<bool> routable(count, false);
    std::vector<bool> done(count, false);
    std::map<PortGroupKey, std::vector<std::size_t>> leave_groups;
    std::map<PortGroupKey, std::vector<std::size_t>> enter_groups;
    std::map<NodeId, std::vector<std::size_t>> by_target;
    for (std::size_t i = 0; i < count; ++i) {
        const Edge& edge = graph.edges()[i];
        const Node* from = graph.find(edge.from());
        const Node* to = graph.find(edge.to());
        if (from == nullptr || to == nullptr) {
            continue;
        }
        leave_side[i] = side_towards(from->box().center(), to->box().center());
        enter_side[i] = side_towards(to->box().center(), from->box().center());
        leave_groups[{edge.from(), side_key(leave_side[i])}].push_back(i);
        enter_groups[{edge.to(), side_key(enter_side[i])}].push_back(i);
        if (from != to) {
            by_target[edge.to()].push_back(i);
        }
        routable[i] = true;
    }
    std::size_t routed = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (!routable[i]) {
            continue;
        }
        const Edge& edge = graph.edges()[i];
        const Node* from = graph.find(edge.from());
        const Node* to = graph.find(edge.to());
        const auto leave_it = leave_groups.find({edge.from(), side_key(leave_side[i])});
        const auto enter_it = enter_groups.find({edge.to(), side_key(enter_side[i])});
        const double leave_t =
            leave_it == leave_groups.end() ? 0.5 : fraction_in(leave_it->second, i);
        const double enter_t =
            enter_it == enter_groups.end() ? 0.5 : fraction_in(enter_it->second, i);
        from_anchor[i] = snap(port_anchor(from->box(), leave_side[i], leave_t));
        to_anchor[i] = snap(port_anchor(to->box(), enter_side[i], enter_t));
    }

    // 自环：两端锚点重合，单点折线（不画）。
    for (std::size_t i = 0; i < count; ++i) {
        if (!routable[i] ||
            graph.find(graph.edges()[i].from()) != graph.find(graph.edges()[i].to())) {
            continue;
        }
        graph.edges()[i].set_route({from_anchor[i]});
        done[i] = true;
        ++routed;
    }

    for (const auto& [target, members] : by_target) {
        const Node* to = graph.find(target);
        if (to == nullptr) {
            continue;
        }
        if (members.size() == 1) {
            const std::size_t i = members[0];
            if (done[i]) {
                continue;
            }
            Edge& edge = graph.edges()[i];
            const Node* from = graph.find(edge.from());
            if (from == nullptr) {
                continue;
            }
            const std::vector<Point> one = route_one(*grid, from_anchor[i], to_anchor[i],
                                                    from->box(), to->box(),
                                                    opts.route.max_detour);
            // route_one 只在 Z 形/L 形全部失败时退回 2 点直线兜底 → 视为路由失败（雷区1）。
            edge.set_route(one);
            edge.set_route_failed(one.size() <= 2);
            done[i] = true;
            ++routed;
            continue;
        }

        // 捆绑组：进入侧取"目标中心 → 源质心"的主轴侧；锚点与并束点整组共享。
        Point centroid;
        std::size_t live = 0;
        for (const std::size_t i : members) {
            if (done[i]) {
                continue;
            }
            const Node* from = graph.find(graph.edges()[i].from());
            if (from != nullptr) {
                centroid += from->box().center();
                ++live;
            }
        }
        if (live == 0) {
            continue;
        }
        centroid = centroid / static_cast<double>(live);
        const PortSide entry = side_towards(to->box().center(), centroid);
        const Point anchor = snap(port_anchor(to->box(), entry));
        const Point outward = outward_of(entry);

        // 并束点沿外法向逐步远离，直到落在所有节点框之外。
        Point bundle = anchor;
        bool found = false;
        for (int attempt = 1; attempt <= 16 && !found; ++attempt) {
            const double gap = opts.bundle_gap * static_cast<double>(attempt);
            const Point cand =
                snap(Point{anchor.x + outward.x * gap, anchor.y + outward.y * gap});
            if (cand.x < 0.0 || cand.y < 0.0 || cand.x > kMaxGridCoord ||
                cand.y > kMaxGridCoord) {
                break;
            }
            // 并束点必须落在所有节点框之外：直接查已登记节点框的栅格（O(1)），不再每候选
            // 全量遍历 graph.nodes()（组数×尝试次数×节点数 在高密度图下会爆成 O(G·V)）。
            // 注：grid 里还含已路由连线，occupied 是"节点框"的超集——并束点本就应避免
            // 压在既有连线上，更严但不会比原逻辑更糟（找不到框外点时整组退回普通路由）。
            const bool inside = grid->in_bounds(static_cast<std::uint32_t>(cand.x),
                                               static_cast<std::uint32_t>(cand.y)) &&
                                grid->occupied(static_cast<std::uint32_t>(cand.x),
                                               static_cast<std::uint32_t>(cand.y));
            if (!inside) {
                bundle = cand;
                found = true;
            }
        }

        // 两遍走：先全组定候选（此时组内任何折线都未入栅格，兄弟边之间不做
        // 互斥——共享的干线与末段**重叠正是并束**），再统一占用、写回。
        struct Candidate {
            std::vector<Point> path;
            bool ok = false;
        };
        std::vector<Candidate> picked(members.size());
        for (std::size_t k = 0; k < members.size(); ++k) {
            const std::size_t i = members[k];
            if (done[i]) {
                continue;
            }
            if (!found) {
                continue;  // 找不到框外的并束点：整组放弃捆绑，下面走普通路由
            }
            const Edge& edge = graph.edges()[i];
            const Node* from = graph.find(edge.from());
            if (from == nullptr) {
                continue;
            }
            const Point a = from_anchor[i];
            const Rect& from_box = from->box();
            const Rect& to_box = to->box();
            const bool x_disjoint =
                from_box.right() <= to_box.x || to_box.right() <= from_box.x;
            const bool y_disjoint =
                from_box.bottom() <= to_box.y || to_box.bottom() <= from_box.y;
            bool horizontal =
                std::fabs(bundle.x - a.x) >= std::fabs(bundle.y - a.y);
            if (x_disjoint) {
                horizontal = true;
            } else if (y_disjoint) {
                horizontal = false;
            }
            const double base = horizontal ? (a.x + bundle.x) / 2.0
                                           : (a.y + bundle.y) / 2.0;
            for (int detour = 0; detour <= opts.route.max_detour && !picked[k].ok;
                 ++detour) {
                for (int sign = 1; sign >= -1; sign -= 2) {
                    if (detour == 0 && sign < 0) {
                        continue;
                    }
                    const double mid =
                        std::round(base + static_cast<double>(detour * sign));
                    std::vector<Point> candidate =
                        orthogonal_path(a, bundle, horizontal, mid);
                    if (candidate.back().x != anchor.x ||
                        candidate.back().y != anchor.y) {
                        candidate.push_back(anchor);  // 共享末段：并束点 → 公共锚点
                    }
                    if (can_place(*grid, candidate)) {
                        picked[k].path = std::move(candidate);
                        picked[k].ok = true;
                        break;
                    }
                }
            }
        }
        for (std::size_t k = 0; k < members.size(); ++k) {
            const std::size_t i = members[k];
            if (done[i]) {
                continue;
            }
            Edge& edge = graph.edges()[i];
            const Node* from = graph.find(edge.from());
            if (from == nullptr) {
                continue;
            }
            if (picked[k].ok) {
                place(*grid, picked[k].path);  // 已占用的格静默跳过：共享段只占一次
                edge.set_route(std::move(picked[k].path));
                edge.set_route_failed(false);
            } else {
                const std::vector<Point> one = route_one(*grid, from_anchor[i], to_anchor[i],
                                                        from->box(), to->box(),
                                                        opts.route.max_detour);
                // 整组放弃捆绑、退回普通路由，route_one 仍可能退回直线兜底。
                edge.set_route(one);
                edge.set_route_failed(one.size() <= 2);
            }
            done[i] = true;
            ++routed;
        }
    }
    return routed;
}

}  // namespace termchart::graph
