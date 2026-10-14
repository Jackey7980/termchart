// widget.hpp —— widget 模块主头：保留式控件基类 + 主题色板值类型
//
// 模块定位（结构.md 二十二，层号 L18 见 TermchartModules.cmake）：
//   * Widget 是控件树的最小基类：身份（id）+ 几何（bounds，格坐标，半开区间）
//     + 状态旗标（可见/可用/可聚焦/已聚焦）+ 可覆写的命中测试。
//     它**不定义事件系统**——事件路由归 interaction/，无状态组件见
//     immediate_widget.hpp（即时模式），响应式值见 reactive.hpp。
//   * ThemePalette 是值语义色板：theme_dark/light/mono 各提供一张
//     inline constexpr 表；不可变、无全局状态，可按值传递、可编译期取用。
#ifndef TERMCHART_WIDGET_WIDGET_HPP
#define TERMCHART_WIDGET_WIDGET_HPP

#include <string>
#include <utility>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::widget {

// 主题色板：纯值聚合，语义槽位固定。success/danger 在单色主题下与前景同色
// （亮度无法编码语义，形态差异由组件字符承担，如 checkbox 的 'x'/' '）。
struct ThemePalette {
    Rgb background;    // 面板/控件底色
    Rgb foreground;    // 常规文字
    Rgb border;        // 边框、括号等结构字符
    Rgb accent;        // 强调：聚焦、选中标记、按压反白
    Rgb text_dim;      // 次要/禁用文字
    Rgb success;       // 语义：成功
    Rgb danger;        // 语义：危险
    Rgb highlight_bg;  // 悬停/选区底色

    constexpr bool operator==(const ThemePalette&) const noexcept = default;
    constexpr bool operator!=(const ThemePalette&) const noexcept = default;
};

// 便捷 Style 构造。保持 constexpr：主题表 + 这两个函数可在编译期拼出常量样式。
[[nodiscard]] constexpr Style style_fg(Rgb fg) noexcept {
    Style s{};
    s.set_fg(Color::rgb(fg));
    return s;
}
[[nodiscard]] constexpr Style style_fg_bg(Rgb fg, Rgb bg) noexcept {
    Style s{};
    s.set_fg(Color::rgb(fg));
    s.set_bg(Color::rgb(bg));
    return s;
}

// 控件基类。以身份参与控件树与命中测试：禁止拷贝/移动（父子指针与
// 焦点链都按地址走）。几何与 interaction::immediate 的坐标约定一致——
// 本层不做"格坐标 → 数据坐标"换算，鼠标格坐标原样进入 hit_test。
class Widget {
public:
    explicit Widget(std::string id);
    virtual ~Widget();

    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;
    Widget(Widget&&) = delete;
    Widget& operator=(Widget&&) = delete;

    [[nodiscard]] const std::string& id() const noexcept;

    // bounds 为格坐标矩形（core::Rect 半开区间）；负坐标与空矩形都合法，
    // 命中测试自然会拒绝它们。
    [[nodiscard]] const Rect& bounds() const noexcept;
    void set_bounds(Rect bounds) noexcept;

    [[nodiscard]] bool visible() const noexcept;
    void set_visible(bool on) noexcept;
    [[nodiscard]] bool enabled() const noexcept;
    void set_enabled(bool on) noexcept;

    // set_focus 只在 focusable 时生效：对不可聚焦控件调 set_focus(true) 是
    // no-op 而不是错误（调用方不必先查询）。
    [[nodiscard]] bool focusable() const noexcept;
    void set_focusable(bool on) noexcept;
    [[nodiscard]] bool focused() const noexcept;
    void set_focus(bool on) noexcept;

    // 命中测试：默认 = visible && enabled && bounds.contains。
    // 子类可覆写（圆形按钮、镂空区、整树代理转发等）；覆写者自行决定
    // 是否沿用旗标判定。
    [[nodiscard]] virtual bool hit_test(Point p) const noexcept;

    // 整树重排钩子：默认空实现（叶子控件无需重排）。容器覆写为"先排自己、
    // 再递归子容器"，从而一次 relayout() 拍平整棵控件树。Widget 基类不定义
    // layout（那是容器的事），所以这里只提供安全的递归落点。
    virtual void relayout() noexcept {}

private:
    std::string id_;
    Rect bounds_{};
    bool visible_ = true;
    bool enabled_ = true;
    bool focusable_ = true;
    bool focused_ = false;
};

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_WIDGET_HPP
