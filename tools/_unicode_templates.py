#!/usr/bin/env python3
"""_unicode_templates.py —— gen_unicode_tables.py 的产物模板（tools/ 内部使用）。

每个 render_* 返回完整 .hpp 文本；产物必须能独立编译（scripts/check_headers.sh）。
占位符统一用 @@name@@（C++ 源码里花括号太多，不能用 str.format）。
表内码点一律用十六进制整型常量，避免生成物里出现转义字面量的歧义。
"""
from __future__ import annotations


def _subst(template: str, **kv: str) -> str:
    out = template
    for key, value in kv.items():
        out = out.replace("@@%s@@" % key, value)
    return out


def _cells(codes: list[int]) -> str:
    return ", ".join("0x%04X" % c for c in codes)


def _line(codes: list[int]) -> str:
    return "    " + _cells(codes) + ",\n"


def render_unicode_tables(*, wide: str, ambiguous: str, combining: str, fmt: str) -> str:
    return _subst(UNICODE_TEMPLATE, wide=wide, ambiguous=ambiguous,
                  combining=combining, fmt=fmt)


UNICODE_TEMPLATE = """// unicode_tables.hpp —— 由 tools/gen_unicode_tables.py 生成，禁手改。
//
// 层号 -1：横切数据表，任何层都可 include；不依赖 termchart 其余文件。
// 只回答"码点级朴素宽度"，不含 emoji 序列规则（见 docs/design/emoji_width.md）；
// cluster 宽度由 util/wcwidth.hpp 结合 core/grapheme 数据结构给出。
#ifndef TERMCHART_DETAIL_UNICODE_TABLES_HPP
#define TERMCHART_DETAIL_UNICODE_TABLES_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace termchart::detail {

struct CpRange {
    std::uint32_t first;   // 闭区间起点
    std::uint32_t last;    // 闭区间终点
};

constexpr bool in_any_range(const CpRange* table, std::size_t n, std::uint32_t cp) noexcept {
    std::size_t lo = 0;
    std::size_t hi = n;
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (cp < table[mid].first) {
            hi = mid;
        } else if (cp > table[mid].last) {
            lo = mid + 1;
        } else {
            return true;
        }
    }
    return false;
}

// East Asian Width = Wide / Fullwidth：显示宽度 2。
@@wide@@

// East Asian Width = Ambiguous：策略相关，默认按窄（见 util/wcwidth.hpp）。
@@ambiguous@@

// 组合记号 Mn / Me + 变体选择符 + emoji 肤色修饰符：宽度 0。
@@combining@@

// 格式字符 Cf 与特定零宽控制：宽度 0。
@@fmt@@

constexpr bool is_east_asian_wide(char32_t cp) noexcept {
    return in_any_range(kEastAsianWide.data(), kEastAsianWideCount,
                        static_cast<std::uint32_t>(cp));
}

constexpr bool is_east_asian_ambiguous(char32_t cp) noexcept {
    return in_any_range(kEastAsianAmbiguous.data(), kEastAsianAmbiguousCount,
                        static_cast<std::uint32_t>(cp));
}

constexpr bool is_combining(char32_t cp) noexcept {
    return in_any_range(kCombining.data(), kCombiningCount, static_cast<std::uint32_t>(cp));
}

constexpr bool is_format(char32_t cp) noexcept {
    return in_any_range(kFormat.data(), kFormatCount, static_cast<std::uint32_t>(cp));
}

// C0 / DEL / C1：控制字符，不可直接绘制。
constexpr bool is_control(char32_t cp) noexcept {
    const auto c = static_cast<std::uint32_t>(cp);
    return c < 0x20u || c == 0x7Fu || (c >= 0x80u && c <= 0x9Fu);
}

constexpr bool is_zero_width(char32_t cp) noexcept {
    return is_control(cp) || is_combining(cp) || is_format(cp);
}

// 朴素宽度：0 / 1 / 2。不含 emoji ZWJ 序列规则。
constexpr int naive_width(char32_t cp) noexcept {
    if (is_zero_width(cp)) {
        return 0;
    }
    if (is_east_asian_wide(cp)) {
        return 2;
    }
    return 1;
}

}  // namespace termchart::detail

#endif  // TERMCHART_DETAIL_UNICODE_TABLES_HPP
"""


def render_box_chars(*, light: list[int], heavy: list[int], double: list[int],
                     round_: list[int]) -> str:
    rows = ""
    for label, codes in (("Light", light), ("Heavy", heavy),
                         ("Double", double), ("Round", round_)):
        rows += ("    {" + _cells(codes[:8]) + ",\n     " + _cells(codes[8:]) +
                 "},  // " + label + "\n")
    return _subst(BOX_TEMPLATE, tables=rows)


BOX_TEMPLATE = """// box_chars.hpp —— 由 tools/gen_unicode_tables.py 生成，禁手改。
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
@@tables@@}};
// clang-format on

constexpr char32_t box_char(std::uint8_t mask, BoxWeight weight = BoxWeight::Light) noexcept {
    return kBoxChars[static_cast<std::size_t>(weight)][mask & 0x0Fu];
}

inline constexpr char32_t kHorizontalLight = 0x2500;
inline constexpr char32_t kVerticalLight = 0x2502;

}  // namespace termchart::detail

#endif  // TERMCHART_DETAIL_BOX_CHARS_HPP
"""


def render_block_chars() -> str:
    lower = _line([0x20] + [0x2580 + i for i in range(1, 9)])
    left = _line([0x20] + [0x2590 - i for i in range(1, 9)])
    return _subst(BLOCK_TEMPLATE, lower=lower, left=left)


BLOCK_TEMPLATE = """// block_chars.hpp —— 由 tools/gen_unicode_tables.py 生成，禁手改。
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
@@lower@@}};

// 左侧填充：[0] = 空格，[n] = 宽度占 n/8 的左块。
inline constexpr std::array<char32_t, 9> kLeftBlocks{{
@@left@@}};

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
"""
