// title.hpp —— 标题（居中/左/右对齐到给定矩形）
#ifndef TERMCHART_LAYOUT_TITLE_HPP
#define TERMCHART_LAYOUT_TITLE_HPP

#include <string_view>

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::layout {

class Sink;

TERMCHART_API void draw_title(Sink& sink, std::string_view title, const Rect& rect,
                              HAlign align = HAlign::Center, const Style& style = Style{});

TERMCHART_API void draw_title(canvas::Canvas& canvas, std::string_view title, const Rect& rect,
                              HAlign align = HAlign::Center, const Style& style = Style{});

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_TITLE_HPP
