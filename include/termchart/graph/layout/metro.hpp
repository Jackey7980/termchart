// metro.hpp —— 地铁布局（线网 45° 走线）
//
// 真实地铁路网的画法约定：站点吸附到八线栅格（0°/45°/90°/135°），线路沿
// 45° 斜线与轴对齐段走线。本文件的实现分两层：
//
// **layout_metro（布局）**：把图按无向邻接贪心分解成若干条"线"（链，覆盖全部
// 节点，孤立节点是长度 1 的链），每条线在自己的水平带内沿 45° 之字排列——
// 链上相邻节点的外框原点恰好相差 (step, ±step)（换向处相差 (step, 0)），
// 因此锚点直连天然就是 45° 斜线或水平段。step 会被抬到不小于最大节点框的
// 宽与高，保证相邻框不相交：45° 段只在框外空档里走，**链内走线不穿框**由
// 几何直接保证，不依赖栅格检查。
//
// **route_metro（路由）**：与布局配套的 45° 走线。每条边先试锚点直连（仅当
// 恰为 45° 或轴对齐），再试 45° 步进走线（斜着走到对齐、再轴对齐直进），逐步
// 在栅格上查障碍；不通则退化为正交路由（复用 route_edges 的 Z 形策略），最后
// 退化为直线。跨线边（链与链之间）没有布局层的几何保证，靠这里的栅格检查兜底。
#ifndef TERMCHART_GRAPH_LAYOUT_METRO_HPP
#define TERMCHART_GRAPH_LAYOUT_METRO_HPP

#include <cstddef>

#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"
#include "termchart/graph/route/route.hpp"
#include "termchart/graph/wiregrid.hpp"

namespace termchart::graph {

struct MetroOptions {
    LayoutOptions layout{};  // 节点尺寸与边距
    double step = 6.0;       // 45° 步长（相邻链节点的 x/y 位移）；会被抬到 ≥ 最大框宽高
    std::uint32_t zigzag = 3;  // 之字半周期（每 zigzag 个节点换向一次）
    double line_gap = 3.0;   // 两条线（水平带）之间的间距
};

// 地铁布局：就地写入 Node::set_box，返回所有节点外框的并集；空图返回零矩形。
[[nodiscard]] TERMCHART_API Rect layout_metro(Graph& graph,
                                              const MetroOptions& opts = {});

struct MetroRouteOptions {
    RouteOptions route{};  // 正交兜底时的绕行上限
};

// 45° 走线：为所有边计算折线并写入 Edge::set_route（首尾为节点边框锚点）。
// grid 非空时在其上避让并占用；为空时自建覆盖全图并留绕行余量的栅格。
// 返回写入折线的边数。
[[nodiscard]] TERMCHART_API std::size_t route_metro(Graph& graph,
                                                    const MetroRouteOptions& opts = {},
                                                    WireGrid* grid = nullptr);

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_LAYOUT_METRO_HPP
