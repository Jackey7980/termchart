// border.hpp —— 即时模式边框绘制（header-only，无状态）
//
// 与 immediate_widget.hpp 同风格：不持有状态，调用方传入 canvas + rect +
// 主题，每次调用重描完整边框。box-drawing 字符取自 Unicode，配色用
// ThemePalette.border。矩形不足 2 格宽/高时退化为单边（不崩、不画四角）。
#ifndef TERMCHART_WIDGET_BORDER_HPP
#define TERMCHART_WIDGET_BORDER_HPP

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/widget/widget.hpp"

namespace termchart::widget {

// 边框线型。None 表示不画（调用方自行决定）。
enum class BorderStyle {
    None,
    Single,   // ┌─┐ │ └┘
    Double,   // ╔═╗ ║ ╚╝
    Rounded,  // ╭─╮ │ ╰╯
};

// 用 box-drawing 字符把 rect 描边到 canvas（即时风格，与 immediate_widget 一致）。
// rect 宽高需 >= 2 才能画出四角；不足时退化为单边。
inline void draw_border(canvas::Canvas& c, Rect rect, const ThemePalette& theme,
                        BorderStyle style = BorderStyle::Single) noexcept {
    if (style == BorderStyle::None) {
        return;
    }
    const Style s = style_fg(theme.border);
    const char32_t h = (style == BorderStyle::Double) ? U'═' : U'─';
    const char32_t v = (style == BorderStyle::Double) ? U'║' : U'│';
    const char32_t tl = (style == BorderStyle::Double) ? U'╔'
                      : (style == BorderStyle::Rounded) ? U'╭' : U'┌';
    const char32_t tr = (style == BorderStyle::Double) ? U'╗'
                      : (style == BorderStyle::Rounded) ? U'╮' : U'┐';
    const char32_t bl = (style == BorderStyle::Double) ? U'╚'
                      : (style == BorderStyle::Rounded) ? U'╰' : U'└';
    const char32_t br = (style == BorderStyle::Double) ? U'╝'
                      : (style == BorderStyle::Rounded) ? U'╯' : U'┘';

    const double x0 = rect.x;
    const double y0 = rect.y;
    const double x1 = rect.x + rect.w - 1.0;
    const double y1 = rect.y + rect.h - 1.0;

    // 四角
    c.put(Point{x0, y0}, tl, s);
    c.put(Point{x1, y0}, tr, s);
    c.put(Point{x0, y1}, bl, s);
    c.put(Point{x1, y1}, br, s);

    // 上/下横线
    for (double x = x0 + 1.0; x < x1; ++x) {
        c.put(Point{x, y0}, h, s);
        c.put(Point{x, y1}, h, s);
    }
    // 左/右竖线
    for (double y = y0 + 1.0; y < y1; ++y) {
        c.put(Point{x0, y}, v, s);
        c.put(Point{x1, y}, v, s);
    }
}

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_BORDER_HPP
