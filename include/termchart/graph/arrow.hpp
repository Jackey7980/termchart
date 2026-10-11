// arrow.hpp —— 箭头头部（字符与占格几何）
#ifndef TERMCHART_GRAPH_ARROW_HPP
#define TERMCHART_GRAPH_ARROW_HPP

#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/port.hpp"

namespace termchart::graph {

// 箭头头部字符（按指向方向）：U+25B6 / U+25C0 / U+25B2 / U+25BC。
[[nodiscard]] TERMCHART_API char32_t arrow_head_char(PortSide dir) noexcept;

// 箭头头部占据的格：tip 及其后一格（沿指向的反方向退一格），按远离 tip 排序。
[[nodiscard]] TERMCHART_API std::vector<Point> arrow_head_cells(Point tip, PortSide dir);

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_ARROW_HPP
