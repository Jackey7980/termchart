// ansi_tables.hpp —— ANSI / 控制字符查表（层号 -1 横切数据表）
//
// 数据表职责：C0 控制码 ↔ 脱字符记号（^@、^[…）与助记名，以及 ESC 序列前缀。
// **不负责生成着色序列** —— SGR 的唯一生成点是 render/sgr.hpp（见 ROADMAP 阶段 1）。
// 本文件不依赖 termchart 其余文件；任何层都可 include。
#ifndef TERMCHART_DETAIL_ANSI_TABLES_HPP
#define TERMCHART_DETAIL_ANSI_TABLES_HPP

#include <array>
#include <cstdint>
#include <string_view>

namespace termchart::detail {

// C0 控制码数量：0x00–0x1F 外加 0x7F（DEL）。
inline constexpr int kC0Count = 33;

struct C0Entry {
    std::uint8_t code;
    char32_t caret;          // 脱字符记号里的字母，例如 0x01 -> 'A'
    std::string_view name;   // 助记名，例如 "SOH"
};

// clang-format off
inline constexpr std::array<C0Entry, kC0Count> kC0Table{{
    {0x00, U'@', "NUL"}, {0x01, U'A', "SOH"}, {0x02, U'B', "STX"}, {0x03, U'C', "ETX"},
    {0x04, U'D', "EOT"}, {0x05, U'E', "ENQ"}, {0x06, U'F', "ACK"}, {0x07, U'G', "BEL"},
    {0x08, U'H', "BS"},  {0x09, U'I', "HT"},  {0x0A, U'J', "LF"},  {0x0B, U'K', "VT"},
    {0x0C, U'L', "FF"},  {0x0D, U'M', "CR"},  {0x0E, U'N', "SO"},  {0x0F, U'O', "SI"},
    {0x10, U'P', "DLE"}, {0x11, U'Q', "DC1"}, {0x12, U'R', "DC2"}, {0x13, U'S', "DC3"},
    {0x14, U'T', "DC4"}, {0x15, U'U', "NAK"}, {0x16, U'V', "SYN"}, {0x17, U'W', "ETB"},
    {0x18, U'X', "CAN"}, {0x19, U'Y', "EM"},  {0x1A, U'Z', "SUB"}, {0x1B, U'[', "ESC"},
    {0x1C, U'\\', "FS"}, {0x1D, U']', "GS"},  {0x1E, U'^', "RS"},  {0x1F, U'_', "US"},
    {0x7F, U'?', "DEL"},
}};
// clang-format on

// 是否为需要"可视化转义"的控制码。
constexpr bool is_c0(std::uint8_t code) noexcept {
    return code <= 0x1Fu || code == 0x7Fu;
}

// 控制码 → 助记名；非控制码返回空视图。
constexpr std::string_view c0_name(std::uint8_t code) noexcept {
    for (const C0Entry& e : kC0Table) {
        if (e.code == code) {
            return e.name;
        }
    }
    return {};
}

// 控制码 → 脱字符记号里的字母（0x01 -> 'A'）。非控制码返回 0。
constexpr char32_t c0_caret_letter(std::uint8_t code) noexcept {
    for (const C0Entry& e : kC0Table) {
        if (e.code == code) {
            return e.caret;
        }
    }
    return 0;
}

// ESC 与其序列引入符：不是 SGR，不涉及颜色。
inline constexpr char32_t kEscape = 0x1B;
inline constexpr std::string_view kCsiPrefix = "\x1B[";
inline constexpr std::string_view kOscPrefix = "\x1B]";
inline constexpr std::string_view kSt = "\x1B\\";

}  // namespace termchart::detail

#endif  // TERMCHART_DETAIL_ANSI_TABLES_HPP
