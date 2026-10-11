// block_chars.hpp —— 由 tools/gen_unicode_tables.py 生成，禁手改。
//
// 层号 -1：横切数据表，任何层都可 include；不依赖 termchart 其余文件。
// 块元素（U+2580 系）的唯一来源：raster/block、chart/progress、indicator/* 共用。
#ifndef TERMCHART_DETAIL_BLOCK_CHARS_HPP
#define TERMCHART_DETAIL_BLOCK_CHARS_HPP

#include <array>
#include <cstdint>

namespace termchart::detail {

inline constexpr char32_t kSpace = 0x20;
inline constexpr char32_t kUpperHalf = 0x2580;
inline constexpr char32_t kLowerHalf = 0x2584;
inline constexpr char32_t kLeftHalf = 0x258C;
inline constexpr char32_t kRightHalf = 0x2590;
inline constexpr char32_t kFullBlock = 0x2588;
inline constexpr char32_t kLightShade = 0x2591;
inline constexpr char32_t kMediumShade = 0x2592;
inline constexpr char32_t kDarkShade = 0x2593;

// 下沿填充：[0] = 空格，[n] = 高度占 n/8 的下块。
inline constexpr std::array<char32_t, 9> kLowerBlocks{{
    0x0020, 0x2581, 0x2582, 0x2583, 0x2584, 0x2585, 0x2586, 0x2587, 0x2588,
}};

// 左侧填充：[0] = 空格，[n] = 宽度占 n/8 的左块。
inline constexpr std::array<char32_t, 9> kLeftBlocks{{
    0x0020, 0x258F, 0x258E, 0x258D, 0x258C, 0x258B, 0x258A, 0x2589, 0x2588,
}};

// 密度阶梯（raster/ascii 用）：由淡到浓。
inline constexpr std::array<char32_t, 10> kDensityChars{{
    0x20, 0x2E, 0x3A, 0x2D, 0x3D, 0x2B, 0x2A, 0x23, 0x25, 0x40,
}};

constexpr char32_t lower_eighth(int n) noexcept {
    const int clamped = n < 0 ? 0 : (n > 8 ? 8 : n);
    return kLowerBlocks[static_cast<std::size_t>(clamped)];
}

constexpr char32_t left_eighth(int n) noexcept {
    const int clamped = n < 0 ? 0 : (n > 8 ? 8 : n);
    return kLeftBlocks[static_cast<std::size_t>(clamped)];
}

constexpr char32_t density_char(double density) noexcept {
    if (!(density > 0.0)) {
        return kSpace;
    }
    const double scaled = density * 9.0 + 0.5;
    const int i = scaled > 9.0 ? 9 : static_cast<int>(scaled);
    return kDensityChars[static_cast<std::size_t>(i)];
}

}  // namespace termchart::detail

#endif  // TERMCHART_DETAIL_BLOCK_CHARS_HPP
