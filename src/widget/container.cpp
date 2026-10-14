// container.cpp —— 容器基类实现
#include "termchart/widget/container.hpp"

namespace termchart::widget {

Container::Container(std::string id) : Widget(std::move(id)) {}
Container::~Container() = default;

void Container::add(Widget& child) { add(&child); }

void Container::add(Widget* child) {
    if (!child) {
        return;
    }
    for (auto* c : children_) {
        if (c == child) {
            return;  // 去重
        }
    }
    children_.push_back(child);
    set_dirty();
}

bool Container::remove(Widget& child) noexcept {
    for (auto it = children_.begin(); it != children_.end(); ++it) {
        if (*it == &child) {
            children_.erase(it);
            set_dirty();
            return true;
        }
    }
    return false;
}

void Container::clear() noexcept {
    children_.clear();
    set_dirty();
}

bool Container::hit_test(Point p) const noexcept {
    if (!visible() || !enabled()) {
        return false;
    }
    for (auto* c : children_) {
        if (c && c->visible() && c->hit_test(p)) {
            return true;
        }
    }
    return bounds().contains(p);
}

void Container::relayout() noexcept {
    layout();
    set_dirty(false);
    for (auto* c : children_) {
        if (c) {
            c->relayout();
        }
    }
}

}  // namespace termchart::widget
