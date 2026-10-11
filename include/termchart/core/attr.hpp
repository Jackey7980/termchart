// attr.hpp —— 字符属性位（header-only）
//
// 属性只有这五个：Bold / Dim / Italic / Underline / Reverse。
// 位运算全部 constexpr，便于编译期组合常量样式。
#ifndef TERMCHART_CORE_ATTR_HPP
#define TERMCHART_CORE_ATTR_HPP

#include <cstdint>

namespace termchart {

enum class Attr : std::uint16_t {
    None = 0,
    Bold = 1u << 0,
    Dim = 1u << 1,
    Italic = 1u << 2,
    Underline = 1u << 3,
    Reverse = 1u << 4,
};

using AttrMask = std::uint16_t;

constexpr AttrMask mask_of(Attr a) noexcept { return static_cast<AttrMask>(a); }
constexpr bool has_flag(AttrMask m, Attr a) noexcept { return (m & mask_of(a)) != 0; }

constexpr AttrMask operator|(Attr a, Attr b) noexcept {
    return static_cast<AttrMask>(static_cast<AttrMask>(a) | static_cast<AttrMask>(b));
}
constexpr AttrMask operator|(AttrMask m, Attr a) noexcept {
    return static_cast<AttrMask>(m | static_cast<AttrMask>(a));
}
constexpr AttrMask operator|(Attr a, AttrMask m) noexcept {
    return static_cast<AttrMask>(m | static_cast<AttrMask>(a));
}
constexpr AttrMask& operator|=(AttrMask& m, Attr a) noexcept {
    m = m | a;
    return m;
}
constexpr AttrMask operator&(AttrMask m, Attr a) noexcept {
    return static_cast<AttrMask>(m & static_cast<AttrMask>(a));
}
// 去掉某一位。注：AttrMask 是整型别名，不能重载 operator~（非类类型），
// 因此需要显式命名函数而不是运算符。
constexpr AttrMask without(AttrMask m, Attr a) noexcept {
    return static_cast<AttrMask>(m & static_cast<AttrMask>(~static_cast<unsigned>(a)));
}

inline constexpr AttrMask kNoAttrs = 0;

}  // namespace termchart

#endif  // TERMCHART_CORE_ATTR_HPP
