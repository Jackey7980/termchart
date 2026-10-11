// legend.hpp —— 图例：色块 + 名称，水平排布在给定矩形内
#ifndef TERMCHART_LAYOUT_LEGEND_HPP
#define TERMCHART_LAYOUT_LEGEND_HPP

#include <span>
#include <string>

#include "termchart/core/style.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::layout {

struct LegendEntry {
    std::string name;
    Color color = Color::ansi16(7);
};

TERMCHART_API void draw_legend(canvas::Canvas& canvas,
                               std::span<const LegendEntry> entries, const Rect& rect,
                               HAlign halign = HAlign::Left,
                               const Style& base = Style{});

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_LEGEND_HPP
