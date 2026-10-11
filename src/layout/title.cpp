// title.cpp —— 标题实现
#include "termchart/layout/title.hpp"

#include "termchart/canvas/canvas.hpp"
#include "internal.hpp"

namespace termchart::layout {

void draw_title(canvas::Canvas& canvas, std::string_view title, const Rect& rect,
                HAlign align, const Style& style) {
    if (title.empty()) {
        return;
    }
    const double start = align_start(rect.x, rect.right(), title.size(), align);
    canvas.draw_text(Point{start, rect.y}, title, style);
}

}  // namespace termchart::layout
