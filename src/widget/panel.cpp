// panel.cpp —— 带边框容器实现
#include "termchart/widget/panel.hpp"

namespace termchart::widget {

Panel::Panel(std::string id) : Container(std::move(id)) {}

Rect Panel::content_rect() const noexcept {
    const Rect b = bounds();
    double t = 0.0;
    if (border_ == BorderStyle::Single) {
        t = 1.0;
    } else if (border_ == BorderStyle::Double) {
        t = 2.0;
    }
    return Rect{b.x + t, b.y + t, b.w - 2.0 * t, b.h - 2.0 * t};
}

void Panel::layout() {
    const Rect cr = content_rect();
    for (auto* c : children_) {
        if (c) {
            c->set_bounds(cr);
        }
    }
    // 子容器由 Container::relayout 的递归落点负责继续排。
}

}  // namespace termchart::widget
