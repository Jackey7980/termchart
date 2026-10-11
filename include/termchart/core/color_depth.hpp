// color_depth.hpp —— 终端颜色能力档位（header-only）
//
// 与具体后端无关：它描述"终端能吃下多少颜色"，由 term/caps.hpp 产出，
// 被 render/sgr.hpp 与 color/quantize.hpp 消费。
#ifndef TERMCHART_CORE_COLOR_DEPTH_HPP
#define TERMCHART_CORE_COLOR_DEPTH_HPP

#include <cstdint>

#include "termchart/core/rgb.hpp"

namespace termchart {

// 升序：档位越高能力越强，可用关系运算符比较。
enum class ColorDepth : std::uint8_t {
    None = 0,      // 无色（不含 SGR 颜色参数）
    Ansi16 = 1,
    Ansi256 = 2,
    TrueColor = 3,
};

constexpr int color_depth_count(ColorDepth depth) noexcept {
    switch (depth) {
        case ColorDepth::None:
            return 1;
        case ColorDepth::Ansi16:
            return 16;
        case ColorDepth::Ansi256:
            return 256;
        case ColorDepth::TrueColor:
            return 16777216;
    }
    return 1;
}

constexpr bool operator<(ColorDepth a, ColorDepth b) noexcept {
    return static_cast<std::uint8_t>(a) < static_cast<std::uint8_t>(b);
}
constexpr bool operator<=(ColorDepth a, ColorDepth b) noexcept {
    return static_cast<std::uint8_t>(a) <= static_cast<std::uint8_t>(b);
}
constexpr bool operator>(ColorDepth a, ColorDepth b) noexcept {
    return static_cast<std::uint8_t>(a) > static_cast<std::uint8_t>(b);
}
constexpr bool operator>=(ColorDepth a, ColorDepth b) noexcept {
    return static_cast<std::uint8_t>(a) >= static_cast<std::uint8_t>(b);
}

// 目标色在给定档位下是否需要降级。Rgb 色在 TrueColor 下无需降级。
constexpr bool ensures_at_least(ColorDepth have, ColorDepth want) noexcept { return have >= want; }

}  // namespace termchart

#endif  // TERMCHART_CORE_COLOR_DEPTH_HPP
