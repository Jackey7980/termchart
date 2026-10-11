// color_fallback.cpp —— 颜色降级桥接
#include "termchart/term/color_fallback.hpp"

#include "termchart/color/quantize.hpp"

namespace termchart::term {

ColorDepth depth_of(const TerminalCaps& caps) noexcept {
    return caps.color_depth;
}

Color apply_fallback(const Color& color, const TerminalCaps& caps) noexcept {
    return color::degrade(color, caps.color_depth);
}

Style apply_fallback(const Style& style, const TerminalCaps& caps) noexcept {
    // Style 的降级也在 quantize 里（它知道如何分别处理前/背景色并保留属性位）。
    return color::degrade(style, caps.color_depth);
}

}  // namespace termchart::term
