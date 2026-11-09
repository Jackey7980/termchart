// obstacle.cpp —— 障碍避让路由实现（登记外部障碍 + 复用正交路由 + 兜底修复）
#include "termchart/graph/route/obstacle.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>  // std::abs(long long)：llabs 不是 std 命名空间的标准名字
#include <utility>
#include <vector>

namespace termchart::graph {

namespace {

constexpr double kMaxGridCoord = 1.0e9;
// BFS 兜底修复的栅格规模上限：超出即放弃修复（维持 best-effort），避免在超大
// 栅格上出现内存/时间爆炸。
constexpr std::size_t kMaxRepairCells = 4u * 1024u * 1024u;

// 把矩形（可带小数、可为负）占据的整格登记为障碍：覆盖 [x, right) x [y, bottom)
// 触及的所有格，越界部分裁剪。
void block_rect(const Rect& r, WireGrid& grid) {
    if (!(r.w > 0.0) || !(r.h > 0.0) || r.right() <= 0.0 || r.bottom() <= 0.0) {
        return;
    }
    const double x0 = std::max(0.0, r.x);
    const double y0 = std::max(0.0, r.y);
    const double x1 = std::min(kMaxGridCoord, r.right());
    const double y1 = std::min(kMaxGridCoord, r.bottom());
    const auto gx0 = static_cast<std::uint32_t>(x0);
    const auto gy0 = static_cast<std::uint32_t>(y0);
    const auto gx1 = static_cast<std::uint32_t>(std::ceil(x1));
    const auto gy1 = static_cast<std::uint32_t>(std::ceil(y1));
    for (std::uint32_t y = gy0; y < gy1 && y < grid.height(); ++y) {
        for (std::uint32_t x = gx0; x < gx1 && x < grid.width(); ++x) {
            (void)grid.occupy(x, y);
        }
    }
}

using Cell = std::pair<std::uint32_t, std::uint32_t>;

// 把折线展平成栅格格序列（连续重复去重）。与 route.cpp 的 path_cells 不同，
// 斜线段（直线兜底产物）按最大轴向步数采样近似，而不是判为非法——兜底直线
// 恰恰是需要被检出"穿障"再修复的对象。越界坐标被丢弃。
std::vector<Cell> flatten_route(const std::vector<Point>& route) {
    std::vector<Cell> cells;
    auto push = [&cells](double x, double y) {
        if (x < 0.0 || y < 0.0 || x > kMaxGridCoord || y > kMaxGridCoord) {
            return;
        }
        const Cell cell{static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)};
        if (cells.empty() || cells.back() != cell) {
            cells.push_back(cell);
        }
    };
    for (std::size_t i = 1; i < route.size(); ++i) {
        const Point& p0 = route[i - 1];
        const Point& p1 = route[i];
        const double span = std::max(std::fabs(p1.x - p0.x), std::fabs(p1.y - p0.y));
        const auto steps = static_cast<std::uint32_t>(std::ceil(span));
        if (steps == 0) {
            continue;
        }
        for (std::uint32_t s = 0; s <= steps; ++s) {
            const double t = static_cast<double>(s) / static_cast<double>(steps);
            push(std::round(p0.x + (p1.x - p0.x) * t), std::round(p0.y + (p1.y - p0.y) * t));
        }
    }
    return cells;
}

// 折线（首末锚点格除外）是否撞上栅格中已登记的占用（节点框 / 外扩障碍 / 其它
// 连线）。route_edges 只在 Z 形与 L 形候选通过占用检查后采纳，因此这条检查
// 实际只会对"直线兜底"的折线报撞击——那正是未兑现不穿障契约的部分。
bool route_hits_blocked(const WireGrid& grid, const std::vector<Point>& route) {
    const std::vector<Cell> cells = flatten_route(route);
    if (cells.size() <= 2) {
        return false;
    }
    for (std::size_t i = 1; i + 1 < cells.size(); ++i) {
        if (!grid.in_bounds(cells[i].first, cells[i].second) ||
            grid.occupied(cells[i].first, cells[i].second)) {
            return true;
        }
    }
    return false;
}

// BFS 复用缓冲：parent/gen 只在尺寸增长时一次性重分配（坑位A——不每条边重分配/清零）；
// gen 用 epoch 戳即可免清零区分"是否已访问"。
struct BfsScratch {
    std::vector<std::int32_t> parent;
    std::vector<std::int32_t> gen;
    std::int32_t epoch = 0;
};

// BFS 找一条绕开所有占用格的轴对齐路径（起终点是节点边框上的锚点格，允许其
// 本身被占用）。找不到返回空。邻居展开顺序固定（右、左、下、上），同一张栅格
// 上结果确定。max_cells 限制探索格数（雷区3·坑位B：防止把"稍穿一个节点"的线
// 绕成绕整屏的巨型凹形），亦限制最坏耗时。
std::vector<Point> bfs_around(const WireGrid& grid, Point a, Point b, std::size_t max_cells,
                             BfsScratch& scratch) {
    const std::uint32_t w = grid.width();
    const std::uint32_t h = grid.height();
    if (w == 0 || h == 0 ||
        static_cast<std::size_t>(w) * static_cast<std::size_t>(h) > kMaxRepairCells) {
        return {};
    }
    const auto to_cell = [](Point p) -> Cell {
        return {static_cast<std::uint32_t>(std::round(p.x)),
                static_cast<std::uint32_t>(std::round(p.y))};
    };
    const Cell start = to_cell(a);
    const Cell goal = to_cell(b);
    if (!grid.in_bounds(start.first, start.second) ||
        !grid.in_bounds(goal.first, goal.second)) {
        return {};
    }
    if (start == goal) {
        return {a, b};
    }

    // parent[i]：-2 未访问，-1 为起点，其余为前驱格的线性下标。
    const std::size_t cells_n = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    // 复用 scratch：尺寸不够才扩（一次性到 cells_n）；gen 用 epoch 戳免清零（坑位A）。
    if (scratch.parent.size() < cells_n) {
        scratch.parent.assign(cells_n, -1);
    }
    if (scratch.gen.size() < cells_n) {
        scratch.gen.assign(cells_n, 0);
    }
    ++scratch.epoch;
    const auto linear = [w](Cell c) {
        return static_cast<std::int32_t>(static_cast<std::size_t>(c.second) * w + c.first);
    };
    std::vector<Cell> queue{start};
    scratch.gen[static_cast<std::size_t>(linear(start))] = scratch.epoch;
    scratch.parent[static_cast<std::size_t>(linear(start))] = -1;
    bool found = false;
    std::size_t expanded = 0;
    for (std::size_t head = 0; head < queue.size() && !found; ++head) {
        const Cell cur = queue[head];
        const std::int32_t dxs[4] = {1, -1, 0, 0};
        const std::int32_t dys[4] = {0, 0, 1, -1};
        for (int d = 0; d < 4; ++d) {
            const std::int64_t nx = static_cast<std::int64_t>(cur.first) + dxs[d];
            const std::int64_t ny = static_cast<std::int64_t>(cur.second) + dys[d];
            if (nx < 0 || ny < 0 || nx >= static_cast<std::int64_t>(w) ||
                ny >= static_cast<std::int64_t>(h)) {
                continue;
            }
            const Cell next{static_cast<std::uint32_t>(nx), static_cast<std::uint32_t>(ny)};
            const std::size_t ni =
                static_cast<std::size_t>(ny) * static_cast<std::size_t>(w) + static_cast<std::size_t>(nx);
            if (scratch.gen[ni] == scratch.epoch) {
                continue;
            }
            // 中间格必须空闲；终点格是锚点，允许落在占用（节点边框）上。
            if (next != goal && grid.occupied(next.first, next.second)) {
                continue;
            }
            scratch.parent[ni] = linear(cur);
            scratch.gen[ni] = scratch.epoch;  // 入队即标记访问，否则同格被重复展开
            if (next == goal) {
                found = true;
                break;
            }
            queue.push_back(next);
        }
        if (++expanded >= max_cells) {
            break;  // 雷区3·坑位B：超探索预算即放弃，避免绕整屏的巨型凹形
        }
    }
    if (!found) {
        return {};
    }

    std::vector<Cell> path;
    std::int32_t cur = linear(goal);
    while (cur >= 0) {
        const auto y = static_cast<std::uint32_t>(static_cast<std::size_t>(cur) / w);
        const auto x = static_cast<std::uint32_t>(static_cast<std::size_t>(cur) % w);
        path.emplace_back(x, y);
        cur = scratch.parent[static_cast<std::size_t>(cur)];
    }
    std::reverse(path.begin(), path.end());

    // 只留拐点：共线中间点化简掉，折线形态与其余正交路由一致。
    std::vector<Point> points;
    points.reserve(path.size());
    for (const Cell& c : path) {
        points.push_back(Point{static_cast<double>(c.first), static_cast<double>(c.second)});
    }
    std::vector<Point> compact;
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (i > 0 && i + 1 < points.size()) {
            const bool same_x = points[i - 1].x == points[i].x &&
                                points[i].x == points[i + 1].x;
            const bool same_y = points[i - 1].y == points[i].y &&
                                points[i].y == points[i + 1].y;
            if (same_x || same_y) {
                continue;
            }
        }
        compact.push_back(points[i]);
    }
    return compact;
}

// 兜底修复：route_edges 的 Z 形 / L 形全部失败时会退化为锚点直连（不做占用检
// 查），这条直线可能穿过外扩障碍甚至节点框——横挡直连走廊或 Z 形起始段贴着
// 源框边框列走时必然发生。对每条中间格撞上占用的折线，用 BFS 在栅格上重找一
// 条绕行正交路径，兑现 route_avoiding"折线不进入节点框或外扩障碍"的契约。
// scratch 跨边复用（坑位A）；max_detour 决定修复预算（坑位B）。
void repair_blocked_routes(Graph& graph, WireGrid& grid, int max_detour, BfsScratch& scratch) {
    for (Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        const bool hit = route_hits_blocked(grid, route);
        if (route.size() < 2 || !hit) {
            continue;
        }
        // 修复预算：直连曼哈顿距离 + 绕行余量；超预算（需绕整屏）即放弃，维持
        // best-effort 直线兜底（route_failed 仍为 true）。雷区3·坑位B。
        const std::size_t manh = static_cast<std::size_t>(
            std::abs(static_cast<long long>(route.front().x - route.back().x)) +
            std::abs(static_cast<long long>(route.front().y - route.back().y)));
        const std::size_t max_cells =
            manh + static_cast<std::size_t>(std::max(0, max_detour)) * 64u + 128u;
        std::vector<Point> detour =
            bfs_around(grid, route.front(), route.back(), max_cells, scratch);
        if (detour.size() < 2) {
            continue;  // 栅格上确实无路可绕：维持 best-effort 直线兜底。
        }
        // 新路径登记进栅格，让后续边的修复与探测绕开它。
        const std::vector<Cell> cells = flatten_route(detour);
        for (std::size_t i = 1; i + 1 < cells.size(); ++i) {
            (void)grid.occupy(cells[i].first, cells[i].second);
        }
        edge.set_route(std::move(detour));
        edge.set_route_failed(false);  // 成功重绕，不再是"失败兜底"
    }
}

}  // namespace

std::size_t route_avoiding(Graph& graph, const std::vector<Rect>& obstacles,
                           const ObstacleOptions& opts, WireGrid* grid_in) {
    if (graph.edges().empty()) {
        return 0;
    }

    WireGrid local;
    WireGrid* grid = grid_in;
    if (grid == nullptr) {
        // 自建栅格：覆盖节点与障碍的并集，再外扩 clearance + 绕行余量。
        double max_right = 0.0;
        double max_bottom = 0.0;
        for (const Node& node : graph.nodes()) {
            const Rect& b = node.box();
            if (b.w > 0.0 && b.h > 0.0) {
                max_right = std::max(max_right, std::min(kMaxGridCoord, b.right()));
                max_bottom = std::max(max_bottom, std::min(kMaxGridCoord, b.bottom()));
            }
        }
        for (const Rect& o : obstacles) {
            if (o.w > 0.0 && o.h > 0.0) {
                max_right = std::max(max_right, std::min(kMaxGridCoord, o.right()));
                max_bottom = std::max(max_bottom, std::min(kMaxGridCoord, o.bottom()));
            }
        }
        const double extra = std::max(0.0, opts.clearance) +
                             static_cast<double>(std::max(0, opts.route.max_detour)) + 2.0;
        double w = std::ceil(max_right) + extra;
        double h = std::ceil(max_bottom) + extra;
        if (w < 1.0) {
            w = 1.0;
        }
        if (h < 1.0) {
            h = 1.0;
        }
        local.resize(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h));
        grid = &local;
    }

    // 障碍外扩 clearance 后登记；负坐标裁剪到 0。
    const double clearance = std::max(0.0, opts.clearance);
    for (const Rect& o : obstacles) {
        if (!(o.w > 0.0) || !(o.h > 0.0)) {
            continue;
        }
        block_rect(Rect{o.x - clearance, o.y - clearance, o.w + 2.0 * clearance,
                        o.h + 2.0 * clearance},
                   *grid);
    }

    // 节点框登记与 Z 形逐级绕行都在 route_edges 里（它自己会再登记一次节点框）。
    const std::size_t routed = route_edges(graph, RouteStyle::Orthogonal, opts.route, grid);
    if (grid != nullptr) {
        BfsScratch scratch;  // 跨所有边复用，避免每条边各分配 16MB（坑位A）
        repair_blocked_routes(graph, *grid, opts.route.max_detour, scratch);
    }
    return routed;
}

}  // namespace termchart::graph