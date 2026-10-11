// geom.hpp —— 平面几何：点线距离、线段关系、多边形面积/重心/凸包
//
// 只依赖 core（Point），纯函数、无状态、无 I/O。`graph/crossing` 的严格相交判定
// 与这里是同一套规则，图模块直接复用本文件，不再各写一份。
#ifndef TERMCHART_UTIL_GEOM_HPP
#define TERMCHART_UTIL_GEOM_HPP

#include <cstddef>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/export.hpp"

namespace termchart::util {

// 叉积的 z 分量：cross(a, b, c) = (b - a) × (c - a)。
// > 0 表示 a→b→c 左转（逆时针），< 0 右转，== 0 三点共线。
[[nodiscard]] TERMCHART_API double cross(Point a, Point b, Point c) noexcept;

// 点到**无限直线** ab 的带符号距离：> 0 表示 p 在 a→b 的左侧。
// a == b 时返回 p 到该点的距离（恒为非负）。
[[nodiscard]] TERMCHART_API double signed_distance_to_line(Point p, Point a, Point b) noexcept;

// 点到**线段** ab 的最近距离（线段退化为点时即两点距离）。
[[nodiscard]] TERMCHART_API double distance_to_segment(Point p, Point a, Point b) noexcept;

// 线段 ab 与 cd 是否**真相交**：共端点、共线重叠、端点落在另一条线段上都**不算**。
// 这正是"连线交叉数"需要的口径（贴着端点绕过去不算交叉）。
[[nodiscard]] TERMCHART_API bool segments_cross(Point a, Point b, Point c, Point d) noexcept;

// 两条线段所在直线的交点：仅在唯一交点落在两条线段之内（含端点）时返回 true。
// 平行、共线、退化线段一律返回 false。
[[nodiscard]] TERMCHART_API bool segment_intersection(Point a, Point b, Point c, Point d,
                                                      Point& out) noexcept;

// 多边形面积（鞋带公式）。顶点按给定顺序；逆时针为正，顺时针为负。
// count < 3 返回 0。
[[nodiscard]] TERMCHART_API double polygon_area(const Point* points,
                                                std::size_t count) noexcept;

// 多边形重心（面积加权）。面积为 0 时退化为顶点算术平均。
[[nodiscard]] TERMCHART_API Point polygon_centroid(const Point* points,
                                                   std::size_t count) noexcept;

// 点是否落在多边形内部（射线法，奇偶规则）。边界上的点视为**在内部**。
[[nodiscard]] TERMCHART_API bool point_in_polygon(Point p, const Point* points,
                                                  std::size_t count) noexcept;

// 凸包（Andrew 单调链）。返回**逆时针**顶点序列，起点为最左下的点；
// 同一条边上的中间共线点会被剔除。点数 < 3 时原样返回（去重后）。
[[nodiscard]] TERMCHART_API std::vector<Point> convex_hull(std::vector<Point> points);

}  // namespace termchart::util

#endif  // TERMCHART_UTIL_GEOM_HPP
