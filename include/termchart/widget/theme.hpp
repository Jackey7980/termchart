// theme.hpp —— 主题聚合 + 按名取用（header-only）
//
// Theme 把名字与 ThemePalette 绑在一起，theme_by_name() 提供按名取用的薄入口
// （dark/light/mono），语义色板常量来自 theme_dark/light/mono。无注册表、无全局
// 状态——要扩展主题就再加一张 constexpr 表并在这个 switch 里加一行。
#ifndef TERMCHART_WIDGET_THEME_HPP
#define TERMCHART_WIDGET_THEME_HPP

#include <string>
#include <string_view>

#include "termchart/widget/theme_dark.hpp"
#include "termchart/widget/theme_light.hpp"
#include "termchart/widget/theme_mono.hpp"
#include "termchart/widget/widget.hpp"

namespace termchart::widget {

// 主题 = 名字 + 色板。
struct Theme {
    std::string name;
    ThemePalette palette;
};

// 按名取主题；未知名回落 dark。
[[nodiscard]] inline Theme theme_by_name(std::string_view name) noexcept {
    if (name == "light") {
        return Theme{"light", kLightTheme};
    }
    if (name == "mono") {
        return Theme{"mono", kMonoTheme};
    }
    return Theme{"dark", kDarkTheme};
}

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_THEME_HPP
