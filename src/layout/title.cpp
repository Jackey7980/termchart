// title.cpp —— 标题实现
#include "termchart/layout/title.hpp"

#include "internal.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/layout/canvas_sink.hpp"
#include "termchart/layout/sink.hpp"

namespace termchart::layout {

void draw_title(Sink& sink, std::string_view title, const Rect& rect, HAlign align,
                const Style& style) {
    if (title.empty()) {
        return;
    }
    const double start = align_start(rect.x, rect.right(), title.size(), align);
    sink.text(Point{start, rect.y}, title, style);
}

void draw_title(canvas::Canvas& canvas, std::string_view title, const Rect& rect, HAlign align,
                const Style& style) {
    CanvasSink sink(canvas);
    draw_title(sink, title, rect, align, style);
}

}  // namespace termchart::layout
