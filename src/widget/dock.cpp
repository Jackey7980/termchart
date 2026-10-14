// dock.cpp —— 五区停靠布局实现
#include "termchart/widget/dock.hpp"

namespace termchart::widget {

Dock::Dock(std::string id) : Container(std::move(id)) {}

void Dock::add(DockRegion region, Widget& child) { add(region, &child); }

void Dock::add(DockRegion region, Widget* child) {
    if (!child) {
        return;
    }
    docked_.push_back({region, child});
    Container::add(child);
}

void Dock::layout() {
    Rect r = bounds();

    for (auto& [region, child] : docked_) {
        if (!child || !child->visible()) {
            continue;
        }
        const Rect cb = child->bounds();

        switch (region) {
            case DockRegion::Top: {
                const double h = cb.h > 0.0 ? cb.h : 0.0;
                child->set_bounds(Rect{r.x, r.y, r.w, h});
                r.y += h;
                r.h -= h;
                break;
            }
            case DockRegion::Bottom: {
                const double h = cb.h > 0.0 ? cb.h : 0.0;
                child->set_bounds(Rect{r.x, r.y + r.h - h, r.w, h});
                r.h -= h;
                break;
            }
            case DockRegion::Left: {
                const double w = cb.w > 0.0 ? cb.w : 0.0;
                child->set_bounds(Rect{r.x, r.y, w, r.h});
                r.x += w;
                r.w -= w;
                break;
            }
            case DockRegion::Right: {
                const double w = cb.w > 0.0 ? cb.w : 0.0;
                child->set_bounds(Rect{r.x + r.w - w, r.y, w, r.h});
                r.w -= w;
                break;
            }
            case DockRegion::Center:
                child->set_bounds(Rect{r.x, r.y, r.w, r.h});
                break;
        }
        if (r.w < 0.0) {
            r.w = 0.0;
        }
        if (r.h < 0.0) {
            r.h = 0.0;
        }
    }
}

}  // namespace termchart::widget
