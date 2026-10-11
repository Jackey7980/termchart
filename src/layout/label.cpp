// label.cpp —— 轴名标签实现
#include "termchart/layout/label.hpp"

#include "termchart/canvas/canvas.hpp"
#include "internal.hpp"

namespace termchart::layout {

void draw_axis_label(canvas::Canvas& canvas, std::string_view text, const Rect& rect,
                     Side side, HAlign halign, const Style& style) {
    if (text.empty()) {
        return;
    }
    switch (side) {
        case Side::Bottom: {
            const double start = align_start(rect.x, rect.right(), text.size(), halign);
            canvas.draw_text(Point{start, rect.bottom() + 2.0}, text, style);
            break;
        }
        case Side::Left: {
            const double start = rect.x - 1.0 - static_cast<double>(text.size());
            canvas.draw_text(Point{start, rect.bottom() - 1.0}, text, style);
            break;
        }
        case Side::Right:
        case Side::Top:
            break;
    }
}

}  // namespace termchart::layout
