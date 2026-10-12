#include "termchart/widget/widget.hpp"

namespace termchart::widget {

Widget::Widget(std::string id) : id_(std::move(id)) {}

Widget::~Widget() = default;

const std::string& Widget::id() const noexcept { return id_; }

const Rect& Widget::bounds() const noexcept { return bounds_; }
void Widget::set_bounds(Rect bounds) noexcept { bounds_ = bounds; }

bool Widget::visible() const noexcept { return visible_; }
void Widget::set_visible(bool on) noexcept { visible_ = on; }

bool Widget::enabled() const noexcept { return enabled_; }
void Widget::set_enabled(bool on) noexcept { enabled_ = on; }

bool Widget::focusable() const noexcept { return focusable_; }
void Widget::set_focusable(bool on) noexcept { focusable_ = on; }

bool Widget::focused() const noexcept { return focused_; }
void Widget::set_focus(bool on) noexcept { focused_ = on && focusable_; }

bool Widget::hit_test(Point p) const noexcept {
    return visible_ && enabled_ && bounds_.contains(p);
}

}  // namespace termchart::widget
