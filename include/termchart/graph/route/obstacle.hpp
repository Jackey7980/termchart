// obstacle.hpp —— 障碍避让路由（外部矩形障碍绕行）
//
// 画布上常有不属于图的元素（图例、表格、已画好的面板），连线必须绕开。
// 把"外部障碍"（std::vector<Rect>，画布坐标系）登记进 WireGrid，再走
// route_edges 的正交路由（Z 形逐级绕行，失败退化为直线）。
//
// 约定：障碍四周外扩 clearance 格禁行；负坐标障碍裁剪到 0（WireGrid 是
// 无符号栅格）；grid 为空时自建覆盖全部节点、障碍与绕行余量的栅格。
#ifndef TERMCHART_GRAPH_ROUTE_OBSTACLE_HPP
#define TERMCHART_GRAPH_ROUTE_OBSTACLE_HPP

#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/route/route.hpp"
#include "termchart/graph/wiregrid.hpp"

namespace termchart::graph {

struct ObstacleOptions {
    RouteOptions route{};   // 正交绕行上限
    double clearance = 1.0; // 障碍四周的额外禁行格数
};

// 障碍避让路由：为所有边计算正交折线并写入 Edge::set_route，折线（锚点除外）
// 不进入任何节点框或外扩后的障碍。返回写入折线的边数。
[[nodiscard]] TERMCHART_API std::size_t route_avoiding(
    Graph& graph, const std::vector<Rect>& obstacles, const ObstacleOptions& opts = {},
    WireGrid* grid = nullptr);

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_ROUTE_OBSTACLE_HPP