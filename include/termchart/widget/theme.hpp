// theme.hpp —— 主题聚合 + 按名取用（转发到 color/，既有 include 路径继续可用）
//
// Theme 与 theme_by_name() 已下沉到 **color/（L2）**：图表（chart=L12）也要按名取
// 主题，住在 widget/（L18）的话图表引用不到。这里只保留别名，既有写法一字不改。
#ifndef TERMCHART_WIDGET_THEME_HPP
#define TERMCHART_WIDGET_THEME_HPP

#include "termchart/color/theme.hpp"
#include "termchart/widget/theme_dark.hpp"
#include "termchart/widget/theme_light.hpp"
#include "termchart/widget/theme_mono.hpp"
#include "termchart/widget/widget.hpp"

namespace termchart::widget {

using termchart::color::Theme;
using termchart::color::theme_by_name;

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_THEME_HPP
