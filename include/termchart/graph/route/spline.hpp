// spline.hpp —— 平滑曲线边路由（Catmull-Rom 穿过正交途经点）
//
// 与 route.hpp 的直线/正交一样，输出仍是"格坐标折线"，只是把折线换成平滑曲线：
// 以锚点之间的正交途经点当控制多边形，按弦长自适应采样 Catmull-Rom 曲线，再逐格
// 取整成 **8 连通格序列**（相邻格只差一行/一列/一格对角）。斜向步由既有
// graph/diagrams/internal.hpp 的 segment_char 渲染成 ╱ ╲，横向/纵向渲染成 ─ │，
// 因此本文件只产出几何、不产出字符（与 primitives.hpp 的分工一致）。
//
// 三条刻意约定：
//   * **宁可折线也不穿框**：曲线一旦压到任何节点框内部（或已占用栅格），该候选直接
//     作废，回退到同一控制多边形对应的正交折线；正交也不通时回退直线。曲线在格粒度
//     下必然有台阶，回退折线比斜穿节点好。
//   * 首末点严格等于锚点：控制多边形以锚点起止，采样后又显式钉死首末格，取整漂移
//     不会传到端点。
//   * grid 为空时自建障碍栅格：不穿框是硬约定，不能因为调用方没给栅格就放弃避让
//     （这点与 route_edges 不同）。
//
// 自环边（A→A）的锚点会重合，spline 不处理自环，请改用 graph/route/arc.hpp。
#ifndef TERMCHART_GRAPH_ROUTE_SPLINE_HPP
#define TERMCHART_GRAPH_ROUTE_SPLINE_HPP

#include <cstddef>

#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/route/route.hpp"
#include "termchart/graph/wiregrid.hpp"

namespace termchart::graph {

// 为所有边计算平滑曲线并写入 Edge::set_route（首尾为节点边框上的锚点）。
// 参数语义与 route_edges 一致：opts.max_detour 决定途经点向两侧尝试的偏移上限；
// grid 非空时在其上做避让并占用，为空时自建一块覆盖全图的栅格。
// 返回写入折线的边数。
[[nodiscard]] TERMCHART_API std::size_t route_spline(Graph& graph, const RouteOptions& opts = {},
                                                     WireGrid* grid = nullptr);

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_ROUTE_SPLINE_HPP
