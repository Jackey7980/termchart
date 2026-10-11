// title.hpp —— 标题：在矩形顶部居中（或按对齐）绘制
#ifndef TERMCHART_LAYOUT_TITLE_HPP
#define TERMCHART_LAYOUT_TITLE_HPP

#include <string_view>

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::layout {

TERMCHART_API void draw_title(canvas::Canvas& canvas, std::string_view title,
                              const Rect& rect, HAlign align = HAlign::Center,
                              const Style& style = Style{});

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_TITLE_HPP
