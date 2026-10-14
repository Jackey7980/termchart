// widget_tree/widget.hpp —— 控件基类（几何节点 + 样式 + 状态 + 绘制/事件钩子）
//
// 绘制走 TkCanvas（即时模式）：子类覆写 paint 在全局坐标画自己；on_event 处理
// 派发来的事件，返回是否消费。Widget 不认识任何渲染管线（遵守 gui_stack.md）。
#ifndef TERMCHART_GUI_WIDGET_TREE_WIDGET_HPP
#define TERMCHART_GUI_WIDGET_TREE_WIDGET_HPP

#include <string>

#include "termchart/core/style.hpp"
#include "termchart_gui/fwd.hpp"
#include "termchart_gui/widget_tree/node.hpp"

namespace termchart_gui {

enum class WidgetState { Normal, Hover, Active, Disabled };

class TkCanvas;  // 前向声明（绘制接口在 backend_tk/tk_canvas.hpp）

class Widget : public Node {
public:
    explicit Widget(std::string id, Style style = {})
        : Node(std::move(id)), style_(style) {}

    const Style& style() const noexcept { return style_; }
    void set_style(Style s) noexcept { style_ = s; }

    WidgetState state() const noexcept { return state_; }
    void set_state(WidgetState s) noexcept { state_ = s; }

    // 绘制：子类覆写，用 canvas 在全局坐标画自己（默认空）。
    virtual void paint(TkCanvas& canvas) const { (void)canvas; }

    // 事件：返回是否消费（默认不消费）。
    virtual bool on_event(const Event& e) {
        (void)e;
        return false;
    }

    [[nodiscard]] Widget* as_widget() noexcept override { return this; }

protected:
    Style style_{};
    WidgetState state_ = WidgetState::Normal;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_WIDGET_HPP
