// layout/layout.hpp —— 布局编排抽象基类
//
// 保留模式布局：measure 求自身需求尺寸，arrange 把子项放进给定矩形（相对容器）。
// GUI 平行栈的容器持有 Layout 子类实例并驱动这两步。
#ifndef TERMCHART_GUI_LAYOUT_LAYOUT_HPP
#define TERMCHART_GUI_LAYOUT_LAYOUT_HPP

#include <vector>

#include "termchart_gui/fwd.hpp"
#include "termchart_gui/layout/constraint.hpp"

namespace termchart_gui {

class Layout {
public:
    virtual ~Layout() = default;

    // 给定可用矩形，返回自身期望尺寸（自然尺寸之和 / 最大交叉尺寸等）。
    [[nodiscard]] virtual Size measure(const Rect& avail,
                                       const std::vector<LayoutItem>& items) const = 0;

    // 把子项几何写回各自的 Widget::set_bounds（相对容器左上角）。
    virtual void arrange(const Rect& rect, std::vector<LayoutItem>& items) = 0;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_LAYOUT_LAYOUT_HPP
