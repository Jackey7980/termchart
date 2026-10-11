// label.hpp —— 轴名等文本标签
#ifndef TERMCHART_LAYOUT_LABEL_HPP
#define TERMCHART_LAYOUT_LABEL_HPP

#include <string_view>

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::layout {

TERMCHART_API void draw_axis_label(canvas::Canvas& canvas, std::string_view text,
                                   const Rect& rect, Side side,
                                   HAlign halign = HAlign::Center,
                                   const Style& style = Style{});

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_LABEL_HPP
