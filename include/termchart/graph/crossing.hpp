// crossing.hpp —— 边交叉计数（布局质量度量）
#ifndef TERMCHART_GRAPH_CROSSING_HPP
#define TERMCHART_GRAPH_CROSSING_HPP

#include <cstddef>
#include <utility>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/export.hpp"

namespace termchart::graph {

// 两条线段是否真正交叉（真交叉）：共端点、共线、仅相切都不算。
[[nodiscard]] TERMCHART_API bool segments_cross(Point a1, Point a2, Point b1,
                                                Point b2) noexcept;

// 一组线段的交叉对数（两两比较，O(n^2)）。用于评估布局/路由质量。
[[nodiscard]] TERMCHART_API std::size_t count_crossings(
    const std::vector<std::pair<Point, Point>>& segments) noexcept;

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_CROSSING_HPP
