// port.hpp —— 节点外框上的连接端口（连线路由的锚点）
#ifndef TERMCHART_GRAPH_PORT_HPP
#define TERMCHART_GRAPH_PORT_HPP

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"

namespace termchart::graph {

enum class PortSide {
    North = 0,
    East,
    South,
    West,
};

// 外框某一侧上的锚点，落在**边框格**上（不含框外），越界坐标已被夹取。
// t 为侧向归一化位置（0 = 该侧起点格，1 = 该侧终点格），按格索引取中点：
// 高 3 的框取 t=0.5 得中间那一行，而不是连续几何上的 1.5 格。
[[nodiscard]] TERMCHART_API Point port_anchor(Rect box, PortSide side,
                                               double t = 0.5) noexcept;

[[nodiscard]] TERMCHART_API PortSide opposite(PortSide side) noexcept;

// 从 from 指向 to 时，最合适的离开/进入侧（按主轴方向判定）。
[[nodiscard]] TERMCHART_API PortSide side_towards(Point from, Point to) noexcept;

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_PORT_HPP
