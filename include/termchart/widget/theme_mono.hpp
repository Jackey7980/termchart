// theme_mono.hpp —— 单色主题：值语义色板常量表（header-only，不可变）
//
// 只用灰阶（r==g==b），给无真彩/无 16 色降级路径与黑白终端用。
// success/danger 与前景同白：亮度编码不了语义，语义差异由组件自己的
// 字符形态承担（如 checkbox 的 'x'/' '、按钮的按压反白）。
#ifndef TERMCHART_WIDGET_THEME_MONO_HPP
#define TERMCHART_WIDGET_THEME_MONO_HPP

#include "termchart/widget/widget.hpp"

namespace termchart::widget {

// clang-format off
inline constexpr ThemePalette kMonoTheme{
    .background   = Rgb{0, 0, 0},
    .foreground   = Rgb{255, 255, 255},
    .border       = Rgb{128, 128, 128},
    .accent       = Rgb{255, 255, 255},
    .text_dim     = Rgb{160, 160, 160},
    .success      = Rgb{255, 255, 255},
    .danger       = Rgb{255, 255, 255},
    .highlight_bg = Rgb{64, 64, 64},
};
// clang-format on

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_THEME_MONO_HPP
