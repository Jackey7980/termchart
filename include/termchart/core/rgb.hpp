// rgb.hpp —— 颜色 POD（header-only，属 cmake/header_only.txt）
//
// 只表达颜色本身，不含"如何输出"：输出编码在 render/sgr.hpp（唯一 SGR 生成点），
// 降级策略在 color/quantize.hpp 与 term/color_fallback.hpp。
#ifndef TERMCHART_CORE_RGB_HPP
#define TERMCHART_CORE_RGB_HPP

#include <cstddef>
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

// ── 编译期字面量（C++20 consteval）────────────────────────────────────────────
// 0xFF0000_rgb   —— 整数十六进制，只取低 24 位
// "#FF0000"_hex  —— 6 位，'#' 可省
// "F00"_hex      —— 3 位缩写，每位按 nibble 展开（F → FF，等价 CSS）
//
// 用 consteval 而不是 constexpr：**打错颜色只能编译期失败**，不存在"运行时
// 才发现"。代价是编译器只报"不是常量表达式"、不会复述下面那几行说明，所以
// 合法形式在这里写死：3 位或 6 位十六进制，前面可带一个 '#'。8 位 RGBA 暂不
// 接受——那要等 color::Rgba 落地，静默丢弃 alpha 比报错更坏。

namespace detail {

constexpr int hex_nibble(char c) noexcept {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;  // 非法字符
}

}  // namespace detail

consteval Rgb operator""_hex(const char* text, std::size_t len) {
    std::size_t n = len;
    if (n > 0 && text[0] == '#') {
        ++text;
        --n;
    }
    if (n != 3 && n != 6) {
        throw "Rgb literal: 需要 \"#RGB\"/\"RGB\"/\"#RRGGBB\"/\"RRGGBB\"";
    }
    const std::size_t per = n == 3 ? 1 : 2;  // 每个分量占几位
    std::uint8_t v[3] = {0, 0, 0};
    for (std::size_t i = 0; i < n; ++i) {
        const int d = detail::hex_nibble(text[i]);
        if (d < 0) {
            throw "Rgb literal: 只接受 0-9/a-f/A-F";
        }
        v[i / per] = static_cast<std::uint8_t>(v[i / per] * 16 + d);
    }
    // 3 位缩写：nibble 要展开成字节（0xF → 0xFF），否则 "#F00" 会变成 0x0F00。
    if (n == 3) {
        for (std::uint8_t& c : v) {
            c = static_cast<std::uint8_t>(c * 17);
        }
    }
    return Rgb{v[0], v[1], v[2]};
}

consteval Rgb operator""_rgb(unsigned long long hex) {
    return Rgb::from_hex(static_cast<std::uint32_t>(hex));
}

inline constexpr Rgb kBlack{0, 0, 0};
inline constexpr Rgb kWhite{255, 255, 255};

}  // namespace termchart

#endif  // TERMCHART_CORE_RGB_HPP
