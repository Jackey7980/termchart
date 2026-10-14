// styles.hpp —— 控件样式值类型 + 基于 ThemePalette 的样式构造（header-only）
//
// WidgetStyle 是一组按交互状态分槽的 Style（normal/hover/active/disabled/
// focused）。它与绘制解耦：即时组件（immediate_widget）或应用层把 WidgetStyle
// 按当前 ControlState 选一份 Style 后交给 canvas。全部 constexpr/inline，无全局
// 状态，可按值传递。
#ifndef TERMCHART_WIDGET_STYLES_HPP
#define TERMCHART_WIDGET_STYLES_HPP

#include "termchart/core/style.hpp"
#include "termchart/widget/widget.hpp"

namespace termchart::widget {

// 交互状态槽位键。与 immediate_widget::ControlState 正交——这里只描述"该状态
// 长什么样"，不描述"怎么进入该状态"。
enum class WidgetState {
    Normal,
    Hover,
    Active,
    Disabled,
    Focused,
};

// 一套控件的五态样式。未显式设置的槽位保持默认 Style（沿用终端默认色）。
struct WidgetStyle {
    Style normal;
    Style hover;
    Style active;
    Style disabled;
    Style focused;
};

// 按钮样式：常态浅字深底，悬停高亮底，按压反白，禁用淡字，聚焦强调字。
[[nodiscard]] inline WidgetStyle button_style(const ThemePalette& t) noexcept {
    WidgetStyle s;
    s.normal   = style_fg_bg(t.foreground, t.background);
    s.hover    = style_fg_bg(t.foreground, t.highlight_bg);
    s.active   = style_fg_bg(t.background, t.accent);
    s.disabled = style_fg_bg(t.text_dim, t.background);
    s.focused  = style_fg_bg(t.accent, t.background);
    return s;
}

// 面板样式：背景填充 + 常态/聚焦两态（边框由 draw_border 单独负责）。
[[nodiscard]] inline WidgetStyle panel_style(const ThemePalette& t) noexcept {
    WidgetStyle s;
    s.normal  = style_fg_bg(t.foreground, t.background);
    s.focused = style_fg_bg(t.accent, t.background);
    return s;
}

// 标签样式：常规前景色，禁用淡色，聚焦强调色。
[[nodiscard]] inline WidgetStyle label_style(const ThemePalette& t) noexcept {
    WidgetStyle s;
    s.normal   = style_fg(t.foreground);
    s.disabled = style_fg(t.text_dim);
    s.focused  = style_fg(t.accent);
    return s;
}

// 按状态取样式槽；未知状态回落 normal。
[[nodiscard]] inline const Style& style_for(const WidgetStyle& ws,
                                            WidgetState st) noexcept {
    switch (st) {
        case WidgetState::Hover:
            return ws.hover;
        case WidgetState::Active:
            return ws.active;
        case WidgetState::Disabled:
            return ws.disabled;
        case WidgetState::Focused:
            return ws.focused;
        case WidgetState::Normal:
        default:
            return ws.normal;
    }
}

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_STYLES_HPP
