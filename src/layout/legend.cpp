// legend.cpp —— 图例实现
#include "termchart/layout/legend.hpp"

#include <cstddef>

#include "termchart/canvas/canvas.hpp"
#include "internal.hpp"

namespace termchart::layout {

void draw_legend(canvas::Canvas& canvas, std::span<const LegendEntry> entries,
                 const Rect& rect, HAlign halign, const Style& base) {
    if (entries.empty()) {
        return;
    }
    std::size_t total = 0;
    for (const auto& e : entries) {
        total += 1 + 1 + e.name.size() + 2;
    }
    if (total > 0) {
        total -= 2;
    }
    const double start = align_start(rect.x, rect.right(), total, halign);
    double x = start;
    const double y = rect.y;
    for (const auto& e : entries) {
        canvas.put(Point{x, y}, U'\u2588', Style{}.set_fg(e.color));  // █
        x += 1.0;
        canvas.put(Point{x, y}, U'\u0020', base);
        x += 1.0;
        canvas.draw_text(Point{x, y}, e.name, base);
        x += static_cast<double>(e.name.size());
        x += 2.0;
    }
}

}  // namespace termchart::layout
