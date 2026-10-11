// arc.hpp —— 弧线边路由（含自环）
//
// 输出与其它路由一致：格坐标折线，且是 **8 连通格序列**（相邻格只差一行/一列/一格
// 对角），斜向步由既有 segment_char 渲染成 ╱ ╲，因此这里只产出几何、不产出字符。
//
// 两种形态：
//   * 普通边（两端是不同节点）：三次贝塞尔弧——控制点沿弦方向外推一格距（保证离开
//     锚点时先朝框**外**走，不会先贴着边框列切进节点）、再按垂直于弦的方向隆起，
//     隆起量约为弦长的一半（"跨列半圆"的观感）。
//   * 自环边（起终点同一节点）：绕到节点框**外侧**的一圈。锚点取在同一个节点相邻
//     两条边上（高框取东侧上下两个锚点，矮框取东侧 + 南侧绕右下角），先水平离框，
//     再把外侧折角用二次贝塞尔抹圆；因为整圈都在框外，节点标签（画在框内）不会被
//     覆盖。
//
// 退化策略（刻意，且可从"不穿框"反推）：候选弧一旦压到任何节点框内部或已占用栅格
// 就作废，依次尝试更小的隆起量 / 两侧方向；都不通则退化为直线（放不下时宁可直连，
// 也不让弧线斜穿节点）。自环放不下时退化为未抹圆的正交外形，再不行退化为锚点直连。
#ifndef TERMCHART_GRAPH_ROUTE_ARC_HPP
#define TERMCHART_GRAPH_ROUTE_ARC_HPP

#include <cstddef>

#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/route/route.hpp"
#include "termchart/graph/wiregrid.hpp"

namespace termchart::graph {

// 为所有边计算弧线并写入 Edge::set_route（首尾为节点边框上的锚点）。
// opts.max_detour 同时充当自环向外绕出的格数与隆起量上限；grid 非空时在其上做
// 避让并占用，为空时自建一块覆盖全图的栅格。返回写入折线的边数。
[[nodiscard]] TERMCHART_API std::size_t route_arc(Graph& graph, const RouteOptions& opts = {},
                                                  WireGrid* grid = nullptr);

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_ROUTE_ARC_HPP
