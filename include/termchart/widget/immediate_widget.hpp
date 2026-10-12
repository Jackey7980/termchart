// immediate_widget.hpp —— 无状态即时组件：按钮 / 复选框
//
// 组件模型（即时模式，参照 interaction/immediate.hpp 的路由思路但不依赖终端）：
//   * **不持有任何状态**：输入旗标（hovered/pressed/...）由调用方每帧从自己的
//     事件流推导后传入，组件每次调用都重画完整外观；
//   * **返回值是"意图"不是"沿"**：hovered && pressed && enabled 即报告
//     clicked/toggled。不做按下→松开的沿检测——那需要记忆，属于状态，
//     调用方比对自己上一帧收到的返回值即可得到沿；
//   * **事件与几何正交**：返回意图只由输入旗标决定（调用方负责用同一个
//     rect 做命中测试），几何只决定绘制——退化几何不绘制、不崩、不影响
//     返回值。这样组件没有"画不出来就算没点中"的隐藏耦合。
//
// 标签按 UTF-8 码点截断（宽度按主码点数估，宽字符按 1 格计；需要精确
// East Asian 宽度时调用方应自行截断后再传入）。
#ifndef TERMCHART_WIDGET_IMMEDIATE_WIDGET_HPP
#define TERMCHART_WIDGET_IMMEDIATE_WIDGET_HPP

#include <string_view>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"
#include "termchart/widget/widget.hpp"

namespace termchart::widget {

// 调用方每帧推导的输入旗标。组件不保存它，传完即忘。
struct ControlState {
    bool hovered = false;
    bool pressed = false;
    bool focused = false;
    bool enabled = true;

    constexpr bool operator==(const ControlState&) const noexcept = default;
    constexpr bool operator!=(const ControlState&) const noexcept = default;
};

// 按钮：填充 face 色 + 居中标签。配色映射：
//   禁用 → text_dim / background；按压(hovered+pressed) → background / accent 反白；
//   悬停 → foreground / highlight_bg；仅聚焦 → accent 文字加粗；常态 → foreground。
// 返回：本轮是否报告点击意图（见文件头注）。
[[nodiscard]] TERMCHART_API bool button_clicked(canvas::Canvas& canvas, Rect rect,
                                                std::string_view label,
                                                const ThemePalette& theme,
                                                const ControlState& state) noexcept;

// 复选框：`[x]`/`[ ]` + 标签，单行顶对齐；标记用 accent（选中）/border（未选中）。
// 返回：本轮是否报告切换意图（调用方据此翻转自己持有的 checked，下一帧传入）。
[[nodiscard]] TERMCHART_API bool checkbox_toggled(canvas::Canvas& canvas, Rect rect,
                                                  std::string_view label, bool checked,
                                                  const ThemePalette& theme,
                                                  const ControlState& state) noexcept;

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_IMMEDIATE_WIDGET_HPP
