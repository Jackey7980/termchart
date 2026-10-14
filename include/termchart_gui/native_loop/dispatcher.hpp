// native_loop/dispatcher.hpp —— 事件路由到控件树
//
// 鼠标事件做命中测试，取最上层 Widget 优先处理；其它事件沿树自顶向下派发，直到
// 某个 Widget 消费。与终端 interaction/ 无关（平行栈自写）。
#ifndef TERMCHART_GUI_NATIVE_LOOP_DISPATCHER_HPP
#define TERMCHART_GUI_NATIVE_LOOP_DISPATCHER_HPP

#include "termchart_gui/fwd.hpp"
#include "termchart_gui/native_loop/event.hpp"
#include "termchart_gui/widget_tree/container.hpp"

namespace termchart_gui {

// 递归找最上层命中控件（全局坐标）。
[[nodiscard]] inline Widget* find_widget_at(Node& n, const Point& p) {
    if (!n.visible() || !n.enabled()) {
        return nullptr;
    }
    if (!n.global_bounds().contains(p)) {
        return nullptr;
    }
    for (auto& c : n.children()) {
        if (Widget* w = find_widget_at(*c, p)) {
            return w;
        }
    }
    return n.as_widget();
}

// 自顶向下派发，直到消费。
[[nodiscard]] inline bool dispatch_tree(Node& n, const Event& e) {
    if (Widget* w = n.as_widget()) {
        if (w->on_event(e)) {
            return true;
        }
    }
    for (auto& c : n.children()) {
        if (dispatch_tree(*c, e)) {
            return true;
        }
    }
    return false;
}

class Dispatcher {
public:
    explicit Dispatcher(Container& root) noexcept : root_(&root) {}

    // 返回是否被某个 Widget 消费。
    [[nodiscard]] bool dispatch(const Event& e) {
        switch (e.type) {
            case EventType::MousePress:
            case EventType::MouseRelease:
            case EventType::MouseMove: {
                Widget* hit = find_widget_at(*root_, Point{e.x, e.y});
                return hit ? hit->on_event(e) : false;
            }
            default:
                return dispatch_tree(*root_, e);
        }
    }

private:
    Container* root_;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_NATIVE_LOOP_DISPATCHER_HPP
