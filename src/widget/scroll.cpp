// scroll.cpp —— 可滚动容器实现
#include "termchart/widget/scroll.hpp"

namespace termchart::widget {

ScrollArea::ScrollArea(std::string id) : Container(std::move(id)) {}

void ScrollArea::set_offset(double x, double y) noexcept {
    const Rect b = bounds();
    off_x_ = x;
    off_y_ = y;
    if (content_w_ > b.w) {
        if (off_x_ > content_w_ - b.w) {
            off_x_ = content_w_ - b.w;
        }
    } else {
        off_x_ = 0.0;
    }
    if (content_h_ > b.h) {
        if (off_y_ > content_h_ - b.h) {
            off_y_ = content_h_ - b.h;
        }
    } else {
        off_y_ = 0.0;
    }
    if (off_x_ < 0.0) {
        off_x_ = 0.0;
    }
    if (off_y_ < 0.0) {
        off_y_ = 0.0;
    }
    set_dirty();
}

void ScrollArea::layout() {
    for (auto* c : children_) {
        if (!c) {
            continue;
        }
        const Rect cb = c->bounds();
        c->set_bounds(Rect{cb.x - off_x_, cb.y - off_y_, cb.w, cb.h});
    }
}

}  // namespace termchart::widget
