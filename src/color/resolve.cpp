// resolve.cpp —— Color / Style → RGB 实现
#include "termchart/color/resolve.hpp"

#include "termchart/color/quantize.hpp"

namespace termchart::color {
namespace {

// 量化后的颜色再取真实 RGB：Ansi16/Indexed 查表，Rgb 原样。
Rgb to_rgb(const Color& color) noexcept {
    switch (color.kind()) {
        case ColorKind::Ansi16:
            return from_index_16(color.index());
        case ColorKind::Indexed:
            return from_index_256(color.index());
        case ColorKind::Rgb:
            return color.rgb();
        case ColorKind::None:
            break;
    }
    return Rgb{};
}

}  // namespace

Rgb resolve_color(const Color& color, const ResolveOptions& options) noexcept {
    if (color.is_none()) {
        return options.default_fg;
    }
    // 先按目标深度量化：深度不足时不假装能显示真彩，GUI 与终端观感因此一致。
    return to_rgb(degrade(color, options.depth));
}

Rgb resolve_background(const Color& color, const ResolveOptions& options) noexcept {
    if (color.is_none()) {
        return options.default_bg;
    }
    return to_rgb(degrade(color, options.depth));
}

ResolvedStyle resolve_style(const Style& style, const ResolveOptions& options) noexcept {
    ResolvedStyle out;
    out.fg = resolve_color(style.fg(), options);
    out.bg_transparent = style.bg().is_none();
    out.bg = resolve_background(style.bg(), options);
    return out;
}

}  // namespace termchart::color
