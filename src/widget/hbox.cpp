// hbox.cpp —— 水平盒子布局实现
#include "termchart/widget/hbox.hpp"

namespace termchart::widget {

HBox::HBox(std::string id) : Container(std::move(id)) {}

void HBox::layout() {
    const Rect outer = bounds();
    const double ix = outer.x + padding_;
    const double iy = outer.y + padding_;
    const double iw = outer.w - 2.0 * padding_;
    const double ih = outer.h - 2.0 * padding_;
    double cursor = ix;

    for (auto* c : children_) {
        if (!c || !c->visible()) {
            continue;
        }
        const Rect cb = c->bounds();
        const double w = cb.w > 0.0 ? cb.w : 0.0;
        double h = cb.h;
        double y = iy;

        if (align_ == Alignment::Stretch) {
            h = ih;
        } else if (align_ == Alignment::Center) {
            y = iy + (ih - h) / 2.0;
        } else if (align_ == Alignment::End) {
            y = iy + ih - h;
        }
        if (h < 0.0) {
            h = 0.0;
        }
        if (y < iy) {
            y = iy;
        }
        c->set_bounds(Rect{cursor, y, w, h});
        cursor += w + spacing_;
    }
    (void)iw;
}

}  // namespace termchart::widget
