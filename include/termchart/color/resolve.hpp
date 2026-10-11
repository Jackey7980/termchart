// resolve.hpp —— Color / Style → 真正的 RGB（GUI 与位图输出的分界点）
//
// 终端侧通常把 `Color` 原样交给 SGR 序列化，由终端去解释（16 色/256 色/真彩）；
// 但 GUI 和位图输出（PNG 截图、sixel 预览）必须拿到**具体像素色**。这个文件就是
// 那道转换，而且是唯一实现：
//   * None  → 用调用方给的默认色（终端默认前景/背景），不是黑色；
//   * Ansi16 / Indexed → 查表；
//   * Rgb   → 先按 ColorDepth 量化再取色，保证"GUI 里看到的颜色"与"终端实际
//     显示的颜色"一致（深度不足时不假装能显示真彩）。
#ifndef TERMCHART_COLOR_RESOLVE_HPP
#define TERMCHART_COLOR_RESOLVE_HPP

#include "termchart/core/color_depth.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::color {

struct ResolveOptions {
    // 目标终端/画布的颜色深度；Rgb 颜色会先量化到该深度再取色。
    ColorDepth depth = ColorDepth::TrueColor;
    // 默认前景（Color 为 None 时使用）。取常见终端的浅灰前景，而不是纯白。
    Rgb default_fg{229, 229, 229};
    // 默认背景（需要背景色但 Style 未指定时使用）。
    Rgb default_bg{20, 20, 20};
};

struct ResolvedStyle {
    Rgb fg{};
    Rgb bg{};
    // Style 未指定背景色：GUI 侧应当与下层混合，而不是填默认背景。
    bool bg_transparent = false;
};

// Color → RGB；None 返回 default_fg。
[[nodiscard]] TERMCHART_API Rgb resolve_color(const Color& color,
                                              const ResolveOptions& options = {}) noexcept;

// Color → RGB；None 返回 default_bg（背景语义的默认值与前景不同）。
[[nodiscard]] TERMCHART_API Rgb resolve_background(
    const Color& color, const ResolveOptions& options = {}) noexcept;

// Style → {fg, bg, bg_transparent}。属性位（加粗/下划线等）不在本函数职责内，
// 调用方仍从原 Style 读取。
[[nodiscard]] TERMCHART_API ResolvedStyle resolve_style(
    const Style& style, const ResolveOptions& options = {}) noexcept;

}  // namespace termchart::color

#endif  // TERMCHART_COLOR_RESOLVE_HPP
