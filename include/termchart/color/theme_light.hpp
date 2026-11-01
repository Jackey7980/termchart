// theme_light.hpp —— 浅色主题：值语义色板常量表（header-only，不可变）
//
// 与 theme_dark 同构，槽位一致；背景亮、文字暗。浅底下的"高亮底色"
// 用淡蓝而不是加深前景，保证与 background 的对比方向一致。
#ifndef TERMCHART_COLOR_THEME_LIGHT_HPP
#define TERMCHART_COLOR_THEME_LIGHT_HPP

#include "termchart/color/theme_palette.hpp"

namespace termchart::color {

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
    .series       = Rgb{30, 32, 38},
    .neutral      = Rgb{52, 96, 168},
    .info         = Rgb{0, 138, 150},
    .caution      = Rgb{176, 128, 0},
};
// clang-format on

}  // namespace termchart::color

#endif  // TERMCHART_COLOR_THEME_LIGHT_HPP
