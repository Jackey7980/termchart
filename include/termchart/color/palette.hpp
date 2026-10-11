// palette.hpp —— 命名调色板
//
// 与 colormap 的区别：**离散**。系列图例、多条折线要的是"彼此可区分的一组色"，
// 而不是连续色带。因此 palette 是一份条目顺序重要的数组（第 i 个系列取第 i 色）。
#ifndef TERMCHART_COLOR_PALETTE_HPP
#define TERMCHART_COLOR_PALETTE_HPP

#include <cstddef>
#include <span>
#include <string_view>

#include "termchart/core/rgb.hpp"
#include "termchart/export.hpp"

namespace termchart::color {

enum class PaletteKind : std::uint8_t {
    Default = 0,   // 深浅背景都可辨的通用色（默认即可用）
    Ocean = 1,
    Forest = 2,
    Sunset = 3,
    Mono = 4,      // 灰度：给单色/打印/降级场景
    HighContrast = 5,
};

inline constexpr int kPaletteCount = 6;

// 未知名字返回 Default，而不是失败：配色不该让图表画不出来。
TERMCHART_API std::span<const Rgb> palette(PaletteKind kind) noexcept;
TERMCHART_API std::span<const Rgb> palette_by_name(std::string_view name) noexcept;

// 第 index 个系列的取色：**循环取模**。系列数超过色数时不越界。
TERMCHART_API Rgb series_color(PaletteKind kind, std::size_t index) noexcept;
TERMCHART_API std::size_t palette_size(PaletteKind kind) noexcept;
TERMCHART_API const char* palette_name(PaletteKind kind) noexcept;

}  // namespace termchart::color

#endif  // TERMCHART_COLOR_PALETTE_HPP
