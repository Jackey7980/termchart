// label_place.hpp —— 标签避让（在锚点周围挑一个不与已有矩形相交的位置）
#ifndef TERMCHART_GRAPH_LABEL_PLACE_HPP
#define TERMCHART_GRAPH_LABEL_PLACE_HPP

#include <cstdint>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"

namespace termchart::graph {

struct LabelPlacement {
    Rect rect{};
    // false 表示四个候选位都与 occupied 相交或越出左/上边界，此时 rect 为首选位。
    bool placed = false;
};

// 候选顺序：右、左、上、下。返回第一个可用位置（不与 occupied 相交，且不越出
// 左/上边界）。occupied 可为空。
[[nodiscard]] TERMCHART_API LabelPlacement place_label(
    Point anchor, std::uint32_t width, std::uint32_t height,
    const std::vector<Rect>& occupied) noexcept;

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_LABEL_PLACE_HPP
