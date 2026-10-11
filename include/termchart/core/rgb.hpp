// rgb.hpp —— 颜色 POD（header-only，属 cmake/header_only.txt）
//
// 只表达颜色本身，不含"如何输出"：输出编码在 render/sgr.hpp（唯一 SGR 生成点），
// 降级策略在 color/quantize.hpp 与 term/color_fallback.hpp。
#ifndef TERMCHART_CORE_RGB_HPP
#define TERMCHART_CORE_RGB_HPP

#include <cstdint>

namespace termchart {

struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    constexpr Rgb() noexcept = default;
    constexpr Rgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue) noexcept
        : r(red), g(green), b(blue) {}

    // 0xRRGGBB → Rgb；越界高位被忽略（只取低 24 位）。
    static constexpr Rgb from_hex(std::uint32_t hex) noexcept {
        return Rgb{static_cast<std::uint8_t>((hex >> 16) & 0xFFu),
                   static_cast<std::uint8_t>((hex >> 8) & 0xFFu),
                   static_cast<std::uint8_t>(hex & 0xFFu)};
    }

    constexpr std::uint32_t to_hex() const noexcept {
        return (static_cast<std::uint32_t>(r) << 16) | (static_cast<std::uint32_t>(g) << 8) |
               static_cast<std::uint32_t>(b);
    }

    // 感知亮度（Rec.709），用于自动挑选对比色。
    constexpr double luma() const noexcept {
        return 0.2126 * static_cast<double>(r) + 0.7152 * static_cast<double>(g) +
               0.0722 * static_cast<double>(b);
    }

    constexpr bool operator==(const Rgb&) const noexcept = default;
    constexpr bool operator!=(const Rgb&) const noexcept = default;
};

inline constexpr Rgb kBlack{0, 0, 0};
inline constexpr Rgb kWhite{255, 255, 255};

}  // namespace termchart

#endif  // TERMCHART_CORE_RGB_HPP
