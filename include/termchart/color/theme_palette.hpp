// theme_palette.hpp —— 全局调色板（值语义，header-only，不可变）
//
// **为什么住在 color/（L2）而不是 widget/**：这份色板要同时驱动 **图表**
// （chart=L12）与 **控件**（widget=L18）。原先它住在 widget/（L18），而分层规则是
// "文件只能 include 层号 ≤ 自己模块的文件"——chart(12) 根本引用不到 widget(18) 的
// 东西，于是"全局主题"实际分裂成两套互不相通的配色：控件走 ThemePalette，图表走
// 另一套 DefaultColor。下沉到 color/(L2) 后两侧都能引用，**一份色板驱动全局**。
//
// 无全局状态、无注册表：色板本身是纯值聚合，用法按值取用（也可整体替换）。
#ifndef TERMCHART_COLOR_THEME_PALETTE_HPP
#define TERMCHART_COLOR_THEME_PALETTE_HPP

#include "termchart/core/rgb.hpp"

namespace termchart::color {

// 主题色板：纯值聚合，语义槽位固定。
//   * 前 8 个槽 = 控件外观（widget）：底色/文字/边框/强调/次要/成功/危险/高亮底色。
//   * 后 4 个槽 = 图表系列色（chart）：数据系列/统计中性/信息色/点缀警示色。
// success/danger 在单色主题下与前景同色（亮度无法编码语义，形态差异由组件字符承担，
// 如 checkbox 的 'x'/' '、按钮的按压反白）；图表侧的四个槽同理在 mono 下取灰阶。
struct ThemePalette {
    Rgb background;    // 面板/控件底色
    Rgb foreground;    // 常规文字
    Rgb border;        // 边框、括号等结构字符
    Rgb accent;        // 强调：聚焦、选中标记、按压反白
    Rgb text_dim;      // 次要/禁用文字
    Rgb success;       // 语义：成功
    Rgb danger;        // 语义：危险
    Rgb highlight_bg;  // 悬停/选区底色
    Rgb series;        // 图表：数据系列（中性前景）
    Rgb neutral;       // 图表：统计类默认
    Rgb info;          // 图表：信息色（金融/极坐标一类）
    Rgb caution;       // 图表：点缀/警示色

    constexpr bool operator==(const ThemePalette&) const noexcept = default;
    constexpr bool operator!=(const ThemePalette&) const noexcept = default;
};

}  // namespace termchart::color

#endif  // TERMCHART_COLOR_THEME_PALETTE_HPP
