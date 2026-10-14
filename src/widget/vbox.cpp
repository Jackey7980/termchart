// vbox.cpp —— 垂直盒子布局实现
#include "termchart/widget/vbox.hpp"

namespace termchart::widget {

VBox::VBox(std::string id) : Container(std::move(id)) {}

void VBox::layout() {
    const Rect outer = bounds();
    const double ix = outer.x + padding_;
    const double iy = outer.y + padding_;
    const double iw = outer.w - 2.0 * padding_;
    const double ih = outer.h - 2.0 * padding_;
    double cursor = iy;

    for (auto* c : children_) {
        if (!c || !c->visible()) {
            continue;
        }
        const Rect cb = c->bounds();
        const double h = cb.h > 0.0 ? cb.h : 0.0;
        double w = cb.w;
        double x = ix;

        if (align_ == Alignment::Stretch) {
            w = iw;
        } else if (align_ == Alignment::Center) {
            x = ix + (iw - w) / 2.0;
        } else if (align_ == Alignment::End) {
            x = ix + iw - w;
        }
        if (w < 0.0) {
            w = 0.0;
        }
        if (x < ix) {
            x = ix;
        }
        c->set_bounds(Rect{x, cursor, w, h});
        cursor += h + spacing_;
    }
    (void)ih;  // 内容溢出视口由调用方裁剪，这里不强行压缩
}

}  // namespace termchart::widget
