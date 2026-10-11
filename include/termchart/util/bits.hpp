// bits.hpp —— 位操作（header-only）
//
// 光栅器（raster/*）与制表符绘制大量依赖位掩码：这里是**唯一**的位操作归口，
// 避免各处手写 `1 << (n - 1)` 而错位。
#ifndef TERMCHART_UTIL_BITS_HPP
#define TERMCHART_UTIL_BITS_HPP

#include <bit>
#include <cstdint>
#include <type_traits>

namespace termchart::util {

// n ∈ [0, 31]，越界返回 0：宁可"不点亮"，也不要 UB。
constexpr std::uint32_t bit32(int n) noexcept { return (n >= 0 && n < 32) ? (1u << n) : 0u; }

template <class T>
constexpr T value_bit(int n) noexcept {
    return static_cast<T>(bit32(n));
}

template <class T>
constexpr int popcount(T v) noexcept {
    using U = std::make_unsigned_t<T>;
    return std::popcount(static_cast<U>(v));
}

template <class T>
constexpr bool test_bit(T v, int n) noexcept {
    using U = std::make_unsigned_t<T>;
    return (static_cast<U>(v) & static_cast<U>(value_bit<U>(n))) != static_cast<U>(0);
}

template <class T>
constexpr T set_bit(T v, int n) noexcept {
    using U = std::make_unsigned_t<T>;
    return static_cast<T>(static_cast<U>(static_cast<U>(v) | static_cast<U>(value_bit<U>(n))));
}

template <class T>
constexpr T clear_bit(T v, int n) noexcept {
    using U = std::make_unsigned_t<T>;
    return static_cast<T>(static_cast<U>(static_cast<U>(v) & ~static_cast<U>(value_bit<U>(n))));
}

// 宽度 width 的低位掩码，例如 low_mask(3) == 0b111。
constexpr std::uint32_t low_mask(int width) noexcept {
    return width <= 0 ? 0u : (width >= 32 ? 0xFFFFFFFFu : ((1u << width) - 1u));
}

// 循环左移：六位打包（sixel）与盲文点阵需要。
constexpr std::uint8_t rotl8(std::uint8_t v, int n) noexcept {
    const int shift = n & 7;
    if (shift == 0) {
        return v;
    }
    return static_cast<std::uint8_t>((v << shift) | (v >> (8 - shift)));
}

}  // namespace termchart::util

#endif  // TERMCHART_UTIL_BITS_HPP
