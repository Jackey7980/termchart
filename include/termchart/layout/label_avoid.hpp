// label_avoid.hpp —— 标签避让（通用实现，位于 layout 层）
//
// 为什么在 layout 而不是 graph：图模块（L13）与图表模块（L12）都要用同一套避让
// 规则，而它们只能向下依赖。把唯一实现放在 layout，上层转引即可 —— 否则同一条
// "候选位顺序 + 不相交 + 不越界"的规则会各写一份，迟早分叉。
//
// 规则（与既有 graph 侧行为完全一致，gap 默认 1 即"紧贴锚点外侧一格"）：
//   * 候选顺序：右、左、上、下；
//   * 候选位必须不与 occupied 中任何矩形相交（半开区间，共边不算相交），
//     且不越出左/上边界与可选的整体 bounds；
//   * 全部不可用时返回首选位并置 placed = false —— 调用方应**放弃绘制**，
//     而不是硬画上去覆盖别人的内容。
#ifndef TERMCHART_LAYOUT_LABEL_AVOID_HPP
#define TERMCHART_LAYOUT_LABEL_AVOID_HPP

#include <cstdint>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"

namespace termchart::layout {

struct LabelBox {
    Point anchor{};
    std::uint32_t width = 0;
    std::uint32_t height = 1;
};

struct LabelAvoidOptions {
    // 标签与锚点之间的间距（格）。
    double gap = 1.0;
    // 允许区域；w 或 h 为 0 表示该方向不限制。
    Rect bounds{};
};

struct LabelSlot {
    Rect rect{};
    // false 表示没有可用位置（rect 为首选位，仅供诊断）。
    bool placed = false;
};

// 单个标签：在锚点四周找第一个可用位置。
[[nodiscard]] TERMCHART_API LabelSlot place_label_avoiding(
    const LabelBox& label, const std::vector<Rect>& occupied,
    const LabelAvoidOptions& options = {}) noexcept;

// 批量：按给定顺序逐个放置；每个成功放置的标签会**自动加入占用集**，
// 因此同批标签之间也不会互相重叠。返回与输入等长的落点列表。
[[nodiscard]] TERMCHART_API std::vector<LabelSlot> place_labels(
    const std::vector<LabelBox>& labels, std::vector<Rect> occupied,
    const LabelAvoidOptions& options = {});

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_LABEL_AVOID_HPP
