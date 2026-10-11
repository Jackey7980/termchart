// sixel_table.hpp —— Sixel 编码字符表（层号 -1 横切数据表）
//
// Sixel 每个 6 位数据值 v ∈ [0,63] 用一个可打印字符 0x3F+v 表示：
//   v=0  -> '?'（空/透明）      v=1  -> '@'     ...  v=63 -> '~'
// 这张表把"值 ↔ 字符"这件事钉死在一处，protocol/sixel.hpp 与本PROTOCOL 旁的
// 解码测试共用同一份定义。
#ifndef TERMCHART_DETAIL_SIXEL_TABLE_HPP
#define TERMCHART_DETAIL_SIXEL_TABLE_HPP

#include <array>
#include <cstdint>

namespace termchart::detail {

inline constexpr char kSixelBase = 0x3F;   // '?'
inline constexpr std::uint8_t kSixelMax = 63;

// clang-format off
inline constexpr std::array<char, 64> kSixelChars{{
    '?', '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^',
    '_', '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~',
}};
// clang-format on

// 数据值 → 字符（越界按取低 6 位处理，等价于终端的解包行为）。
constexpr char sixel_char(std::uint8_t value) noexcept {
    return kSixelChars[value & kSixelMax];
}

// 字符 → 数据值；非法字符返回 0（透明），与"未知一律按空处理"的容错约定一致。
constexpr std::uint8_t sixel_value(char c) noexcept {
    for (std::uint8_t i = 0; i <= kSixelMax; ++i) {
        if (kSixelChars[i] == c) {
            return i;
        }
    }
    return 0;
}

// 常用控制/参数字符（不属于数据值，单独成常量便于读代码）。
inline constexpr char kSixelRepeatIntro = '!';    // DECGRA：重复前一个字符 N 次
inline constexpr char kSixelRasterAttr = '"';     // DECGRA 光栅属性引出符
inline constexpr char kSixelColorIntro = '#';     // DECGCI：选色
inline constexpr char kSixelCrlf = '-';           // 换行（回到行首）
inline constexpr char kSixelNewline = '$';        // 不回到行首的换行

}  // namespace termchart::detail

#endif  // TERMCHART_DETAIL_SIXEL_TABLE_HPP
