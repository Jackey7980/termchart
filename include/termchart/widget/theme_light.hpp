// theme_light.hpp —— 浅色主题：值语义色板常量表（header-only，不可变）
//
// 与 theme_dark 同构，槽位一致；背景亮、文字暗。浅底下的"高亮底色"
// 用淡蓝而不是加深前景，保证与 background 的对比方向一致。
#ifndef TERMCHART_WIDGET_THEME_LIGHT_HPP
#define TERMCHART_WIDGET_THEME_LIGHT_HPP

#include "termchart/widget/widget.hpp"

namespace termchart::widget {

// clang-format off
inline constexpr ThemePalette kLightTheme{
    .background   = Rgb{246, 247, 249},
    .foreground   = Rgb{30, 32, 38},
    .border       = Rgb{200, 204, 214},
    .accent       = Rgb{0, 102, 204},
    .text_dim     = Rgb{110, 116, 128},
    .success      = Rgb{40, 140, 80},
    .danger       = Rgb{200, 50, 50},
    .highlight_bg = Rgb{205, 225, 250},
};
// clang-format on

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_THEME_LIGHT_HPP
