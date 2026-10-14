// panel.hpp —— 带边框的容器（Container + Border）
//
// Panel 在 Container 之上加一层边框：content_rect() 给出减去边框厚度后的内容区，
// 子控件应布局在其中（典型做法是内部再放一个 VBox/HBox）。draw_border() 用
// 即时风格把边框描到 canvas（与 immediate_widget 一致）。Panel 不做子控件打包，
// layout() 仅把子控件整体铺进内容区——嵌套布局容器即可获得真实排版。
#ifndef TERMCHART_WIDGET_PANEL_HPP
#define TERMCHART_WIDGET_PANEL_HPP

#include "termchart/widget/border.hpp"
#include "termchart/widget/container.hpp"

namespace termchart::widget {

class Panel : public Container {
public:
    explicit Panel(std::string id);

    void set_border_style(BorderStyle s) noexcept { border_ = s; set_dirty(); }
    [[nodiscard]] BorderStyle border_style() const noexcept { return border_; }

    // 内容区：bounds 减去边框厚度（None=0，Single=1，Double=2）。
    [[nodiscard]] Rect content_rect() const noexcept;

    // 即时绘制边框（配合 immediate_widget 的使用方式）。
    void draw_border(canvas::Canvas& c, const ThemePalette& theme) const noexcept {
        termchart::widget::draw_border(c, bounds(), theme, border_);
    }

    // 把子控件整体铺进内容区（不打包）。Container::relayout 会再递归子容器。
    void layout() override;

private:
    BorderStyle border_ = BorderStyle::Single;
};

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_PANEL_HPP
