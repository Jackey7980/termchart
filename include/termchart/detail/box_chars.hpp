// box_chars.hpp —— 由 tools/gen_unicode_tables.py 生成，禁手改。
//
// 层号 -1：横切数据表，任何层都可 include；不依赖 termchart 其余文件。
// 制表符（U+2500 系）的唯一来源：graph/wiregrid、layout/box、widget/border 都走这里，
// 不允许在别处硬编码 U+2500 序列。
//
// 邻接掩码约定（bitOR）：Up = 1, Right = 2, Down = 4, Left = 8
#ifndef TERMCHART_DETAIL_BOX_CHARS_HPP
#define TERMCHART_DETAIL_BOX_CHARS_HPP

#include <array>
#include <cstdint>

namespace termchart::detail {

enum class BoxMask : std::uint8_t {
    None = 0,
    Up = 1,
    Right = 2,
    Down = 4,
    Left = 8,
};

constexpr std::uint8_t operator|(BoxMask a, BoxMask b) noexcept {
    return static_cast<std::uint8_t>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

enum class BoxWeight : std::uint8_t {
    Light = 0,
    Heavy = 1,
    Double = 2,
    Round = 3,   // 圆角：仅四个直角掩码有专用码点，其余退化为 Light
};

// clang-format off
// 索引：[weight][mask]
inline constexpr std::array<std::array<char32_t, 16>, 4> kBoxChars{{
    {0x0020, 0x2575, 0x2576, 0x2514, 0x2577, 0x2502, 0x250C, 0x251C,
     0x2574, 0x2518, 0x2500, 0x2534, 0x2510, 0x2524, 0x252C, 0x253C},  // Light
    {0x0020, 0x2579, 0x257A, 0x2517, 0x257B, 0x2503, 0x250F, 0x2523,
     0x2578, 0x251B, 0x2501, 0x253B, 0x2513, 0x252B, 0x2533, 0x254B},  // Heavy
    {0x0020, 0x2551, 0x2550, 0x255A, 0x2551, 0x2551, 0x2554, 0x2560,
     0x2550, 0x255D, 0x2550, 0x2569, 0x2557, 0x2563, 0x2566, 0x256C},  // Double
    {0x0020, 0x2575, 0x2576, 0x2570, 0x2577, 0x2502, 0x256D, 0x251C,
     0x2574, 0x256F, 0x2500, 0x2534, 0x256E, 0x2524, 0x252C, 0x253C},  // Round
}};
// clang-format on

constexpr char32_t box_char(std::uint8_t mask, BoxWeight weight = BoxWeight::Light) noexcept {
    return kBoxChars[static_cast<std::size_t>(weight)][mask & 0x0Fu];
}

inline constexpr char32_t kHorizontalLight = 0x2500;
inline constexpr char32_t kVerticalLight = 0x2502;

}  // namespace termchart::detail

#endif  // TERMCHART_DETAIL_BOX_CHARS_HPP
