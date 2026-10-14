// grid_layout.cpp —— 网格布局实现
#include "termchart/widget/grid_layout.hpp"

namespace termchart::widget {

GridLayout::GridLayout(std::string id) : Container(std::move(id)) {}

void GridLayout::layout() {
    const Rect outer = bounds();
    const double ix = outer.x + padding_;
    const double iy = outer.y + padding_;
    const double iw = outer.w - 2.0 * padding_;
    const double ih = outer.h - 2.0 * padding_;

    std::size_t n = 0;
    for (auto* c : children_) {
        if (c && c->visible()) {
            ++n;
        }
    }
    if (n == 0) {
        return;
    }
    const double cols = static_cast<double>(columns_);
    const double rows = static_cast<double>((n + columns_ - 1) / columns_);
    const double cw = (iw - (cols - 1.0) * spacing_) / cols;
    const double ch = (ih - (rows - 1.0) * spacing_) / rows;

    std::size_t idx = 0;
    for (auto* c : children_) {
        if (!c || !c->visible()) {
            continue;
        }
        const std::size_t col = idx % columns_;
        const std::size_t row = idx / columns_;
        const Rect cell{ix + static_cast<double>(col) * (cw + spacing_),
                        iy + static_cast<double>(row) * (ch + spacing_), cw, ch};
        const Rect cb = c->bounds();
        double w = cb.w;
        double h = cb.h;
        double x = cell.x;
        double y = cell.y;

        if (align_ == Alignment::Stretch) {
            w = cell.w;
            h = cell.h;
        } else if (align_ == Alignment::Center) {
            x = cell.x + (cell.w - w) / 2.0;
            y = cell.y + (cell.h - h) / 2.0;
        } else if (align_ == Alignment::End) {
            x = cell.x + cell.w - w;
            y = cell.y + cell.h - h;
        }
        c->set_bounds(Rect{x, y, w, h});
        ++idx;
    }
}

}  // namespace termchart::widget
