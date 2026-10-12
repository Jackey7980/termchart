// obstacle.cpp —— 障碍避让路由实现（登记外部障碍 + 复用正交路由）
#include "termchart/graph/route/obstacle.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace termchart::graph {

namespace {

constexpr double kMaxGridCoord = 1.0e9;

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
    return route_edges(graph, RouteStyle::Orthogonal, opts.route, grid);
}

}  // namespace termchart::graph