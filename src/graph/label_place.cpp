// label_place.cpp —— 标签避让（图模块侧：转引 layout 的通用实现）
//
// 规则只有一份，在 layout/label_avoid.hpp（L11）：候选顺序右→左→上→下、
// 不与占用矩形相交、不越出左/上边界、全部不可用时不画。graph 是 L13，向下依赖
// 合法；把实现留在这一层会导致图表模块各写一份，规则迟早分叉。
#include "termchart/graph/label_place.hpp"

#include <cstdint>
#include <vector>

#include "termchart/layout/label_avoid.hpp"

namespace termchart::graph {

LabelPlacement place_label(Point anchor, std::uint32_t width, std::uint32_t height,
                           const std::vector<Rect>& occupied) noexcept {
    layout::LabelBox box;
    box.anchor = anchor;
    box.width = width;
    box.height = height;

    const layout::LabelAvoidOptions options;  // 默认 gap = 1，与既有行为一致
    const layout::LabelSlot slot = layout::place_label_avoiding(box, occupied, options);
    return LabelPlacement{slot.rect, slot.placed};
}

}  // namespace termchart::graph
