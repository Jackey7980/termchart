// crossing.cpp —— 线段交叉判定与计数
#include "termchart/graph/crossing.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace termchart::graph {

namespace {

// 叉积符号：> 0 表示 b 在 o->a 的左侧。
double orientation(Point o, Point a, Point b) noexcept {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

bool same_point(Point a, Point b) noexcept {
    return a.x == b.x && a.y == b.y;
}

}  // namespace

bool segments_cross(Point a1, Point a2, Point b1, Point b2) noexcept {
    // 共端点不算交叉：连线的首尾本就落在节点锚点上。
    if (same_point(a1, b1) || same_point(a1, b2) || same_point(a2, b1) ||
        same_point(a2, b2)) {
        return false;
    }
    const double d1 = orientation(b1, b2, a1);
    const double d2 = orientation(b1, b2, a2);
    const double d3 = orientation(a1, a2, b1);
    const double d4 = orientation(a1, a2, b2);
    // 严格异号才算真交叉；共线或端点落在另一段上（叉积为 0）都不计。
    return ((d1 > 0.0 && d2 < 0.0) || (d1 < 0.0 && d2 > 0.0)) &&
           ((d3 > 0.0 && d4 < 0.0) || (d3 < 0.0 && d4 > 0.0));
}

std::size_t count_crossings(
    const std::vector<std::pair<Point, Point>>& segments) noexcept {
    std::size_t count = 0;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        for (std::size_t j = i + 1; j < segments.size(); ++j) {
            if (segments_cross(segments[i].first, segments[i].second, segments[j].first,
                               segments[j].second)) {
                ++count;
            }
        }
    }
    return count;
}

}  // namespace termchart::graph
