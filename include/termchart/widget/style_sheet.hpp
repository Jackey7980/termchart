// style_sheet.hpp —— 类名 → WidgetStyle 映射（header-only，无全局状态）
//
// 即时模式下没有"控件持有自己样式"的说法，样式按类名查表取用：调用方给控件
// 一个 class 名（如 "primary-button"），渲染时从 StyleSheet 取对应 WidgetStyle。
// 与 CSS 的类选择器同构，但只做精确名匹配（不含继承/层叠）。
#ifndef TERMCHART_WIDGET_STYLE_SHEET_HPP
#define TERMCHART_WIDGET_STYLE_SHEET_HPP

#include <string>
#include <string_view>
#include <unordered_map>

#include "termchart/widget/styles.hpp"

namespace termchart::widget {

class StyleSheet {
public:
    // 注册/覆盖一个类样式。
    void add_class(std::string name, WidgetStyle style) {
        classes_.insert_or_assign(std::move(name), std::move(style));
    }

    // 查表；找不到返回 nullptr（调用方据此回落默认样式）。
    [[nodiscard]] const WidgetStyle* find(std::string_view name) const noexcept {
        auto it = classes_.find(std::string(name));
        return it == classes_.end() ? nullptr : &it->second;
    }

    // 取样式：找不到返回 fallback（默认空样式）。
    [[nodiscard]] WidgetStyle style_of(std::string_view name,
                                       const WidgetStyle& fallback = {}) const {
        auto it = classes_.find(std::string(name));
        return it == classes_.end() ? fallback : it->second;
    }

    // 把某类样式整体赋给 into（覆盖）。
    void merge_into(std::string_view name, WidgetStyle& into) const {
        if (const WidgetStyle* p = find(name)) {
            into = *p;
        }
    }

    [[nodiscard]] bool empty() const noexcept { return classes_.empty(); }

private:
    std::unordered_map<std::string, WidgetStyle> classes_;
};

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_STYLE_SHEET_HPP
