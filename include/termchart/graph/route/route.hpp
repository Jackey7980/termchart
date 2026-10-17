// route.hpp —— 连线路由（为每条边算出折线）
//
// 路由的输入是"已布局"的图（节点外框已就位）；输出写进 Edge::set_route。
// 直线是最小实现；正交路由走"Z 形"，并可借助 WireGrid 避让已有连线（先是
// 中点，其次向两侧逐格绕行；全部失败则退化为直线，不占用栅格）。
// 端点在同一行/同一列时，Z 形的共线中间点会被化简掉，结果是一条轴对齐直线段
// （这本身就是正交路由的合法退化，不是错误）。
// 两条约定：同侧多条边按 (节点, 侧) 分组均匀分配锚点，避免共用同一格；锚点
// 吸附到整数格（字符网格上没有半格），绕行中点同样取整。
#ifndef TERMCHART_GRAPH_ROUTE_ROUTE_HPP
#define TERMCHART_GRAPH_ROUTE_ROUTE_HPP

#include <cstddef>

#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/wiregrid.hpp"

namespace termchart::graph {

enum class RouteStyle {
    Straight = 0,  // 锚点直连
    Orthogonal,    // 轴对齐 Z 形折线
};

struct RouteOptions {
    int max_detour = 8;  // 正交路由向两侧尝试的最大偏移（格）
    // 是否把"已路由的连线"也当障碍。默认开：稀疏图里连线互不重叠更好看。
    // 密集图（几十条边挤在层间通道里）建议关：连线之间本就必然相交，硬要
    // 互避会让后来的边找不到任何正交通路、退化成斜线；交叉处由 junction_char
    // 合成 ┼ 表达，比斜线乱穿清楚得多。节点外框无论此开关如何都是障碍。
    bool avoid_wires = true;
    // 尊重已有的折线：布局阶段（layout_sugiyama）会给跨多层的长边写一条沿
    // dummy 槽位走的正交通道折线，比这里的通用 Z 形更准（不会撞中间列的节点框）。
    // 默认为 false：不看已有 route，全部按本函数重算，行为与以前一致。
    bool keep_existing_routes = false;
};

// 为所有边计算折线并写入 Edge::set_route（首尾为节点边框上的锚点）。
// grid 非空且风格为 Orthogonal 时做避让。返回写入折线的边数。
[[nodiscard]] TERMCHART_API std::size_t route_edges(Graph& graph, RouteStyle style,
                                                    const RouteOptions& opts = {},
                                                    WireGrid* grid = nullptr);

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_ROUTE_ROUTE_HPP
