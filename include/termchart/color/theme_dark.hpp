// theme_dark.hpp —— 深色主题：值语义色板常量表（header-only，不可变）
//
// 无全局状态、无注册表：要用就按值取用这张 constexpr 表。色值取向是
// 深底浅字的终端默认观感（背景接近纯黑、强调色取终端常见的蓝）。
#ifndef TERMCHART_COLOR_THEME_DARK_HPP
#define TERMCHART_COLOR_THEME_DARK_HPP

#include "termchart/color/theme_palette.hpp"

namespace termchart::color {

// clang-format off
inline constexpr ThemePalette kDarkTheme{
    .background   = Rgb{16, 18, 24},
    .foreground   = Rgb{220, 224, 232},
    .border       = Rgb{70, 76, 90},
    .accent       = Rgb{86, 156, 214},
    .text_dim     = Rgb{130, 136, 150},
    .success      = Rgb{106, 190, 120},
    .danger       = Rgb{224, 98, 98},
    .highlight_bg = Rgb{38, 79, 120},
    .series       = Rgb{220, 224, 232},
    .neutral      = Rgb{104, 148, 208},
    .info         = Rgb{86, 182, 194},
    .caution      = Rgb{214, 178, 86},
};
// clang-format on

}  // namespace termchart::color

#endif  // TERMCHART_COLOR_THEME_DARK_HPP
