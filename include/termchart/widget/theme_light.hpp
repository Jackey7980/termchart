// theme_light.hpp —— 浅色主题（转发到 color/，既有 include 路径继续可用）
//
// 色板定义已下沉到 **color/（L2）**：它要同时驱动图表（chart=L12）与控件
// （widget=L18），留在 widget/ 里图表引用不到，"全局主题"就分裂成两套。
// 这里只保留别名，`termchart::widget::kLightTheme` 的既有写法一字不改。
#ifndef TERMCHART_WIDGET_THEME_LIGHT_HPP
#define TERMCHART_WIDGET_THEME_LIGHT_HPP

#include "termchart/color/theme_light.hpp"

namespace termchart::widget {

using termchart::color::kLightTheme;

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_THEME_LIGHT_HPP
