// crossing.cpp —— 线段交叉判定与计数
//
// 判定规则只有一份，在 util/geom.hpp（L1）：严格异号才算真交叉，共端点、共线重叠、
// 端点落在另一段上都不算 —— 这正是"连线交叉数"要的口径（贴着端点绕过去不算交叉）。
// 本文件只负责把它按图的连线组织起来做两两计数。
#include "termchart/graph/crossing.hpp"

#include <cstddef>
#include <utility>
#include <vector>

#include "termchart/util/geom.hpp"

namespace termchart::graph {

bool segments_cross(Point a1, Point a2, Point b1, Point b2) noexcept {
    return util::segments_cross(a1, a2, b1, b2);
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
