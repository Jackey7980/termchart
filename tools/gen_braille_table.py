#!/usr/bin/env python3
"""
gen_braille_table.py —— 生成 include/termchart/detail/braille_table.hpp

盲文点阵（dots 1–8）→ U+2800 区码点的映射表。
点 i 对应 bit (i-1)；查表与直接计算等价，但把"点序 ↔ 位序"这件事
钉死在一处，避免各光栅器各自解释（raster/braille.hpp 依赖本表）。

同时给出 2×4 子像素的标准点序常量，供 codec 与光栅器共享：
    0 3      dot1 dot4
    1 4      dot2 dot5
    2 5      dot3 dot6
    6 7      dot7 dot8
"""
from __future__ import annotations

import sys
from pathlib import Path

from _genutil import ROOT, emit, wants_check

OUT = ROOT / "include" / "termchart" / "detail" / "braille_table.hpp"

TEMPLATE = """// braille_table.hpp —— 由 tools/gen_braille_table.py 生成，禁手改。
//
// 层号 -1：横切数据表，任何层都可 include 本文件；本文件不依赖 termchart 其余文件。
// 盲文点阵 vil：**点 i ↔ bit (i-1)**，码点 = U+2800 | dots，这一约定只在本处定义。
#ifndef TERMCHART_DETAIL_BRAILLE_TABLE_HPP
#define TERMCHART_DETAIL_BRAILLE_TABLE_HPP

#include <array>
#include <cstdint>

namespace termchart::detail {{

// 点 i（1..8）的位掩码。
inline constexpr std::uint8_t kBrailleDot1 = 0x01;
inline constexpr std::uint8_t kBrailleDot2 = 0x02;
inline constexpr std::uint8_t kBrailleDot3 = 0x04;
inline constexpr std::uint8_t kBrailleDot4 = 0x08;
inline constexpr std::uint8_t kBrailleDot5 = 0x10;
inline constexpr std::uint8_t kBrailleDot6 = 0x20;
inline constexpr std::uint8_t kBrailleDot7 = 0x40;
inline constexpr std::uint8_t kBrailleDot8 = 0x80;

// 码点 = U+2800 | dots（dots 为上述位掩码的或）。查表仅为显式化该约定。
// clang-format off
inline constexpr std::array<char32_t, 256> kBrailleChars{{
{chars}
}};
// clang-format on

constexpr char32_t braille_char(std::uint8_t dots) noexcept {{
    return kBrailleChars[dots];
}}

// 2×4 子像素网格 → 点掩码。约定几何排布：
//     列 0（左）→ dot1/dot2/dot3/dot7     列 1（右）→ dot4/dot5/dot6/dot8
//     行序自上而下：dot1,dot2,dot3 后接 dot7（左列），dot4,dot5,dot6 后接 dot8（右列）
inline constexpr std::array<std::uint8_t, 8> kBrailleCellDots{{
    kBrailleDot1,  // 左上  (row 0, col 0)
    kBrailleDot2,  // 左中上(row 1, col 0)
    kBrailleDot3,  // 左中下(row 2, col 0)
    kBrailleDot7,  // 左下  (row 3, col 0)
    kBrailleDot4,  // 右上  (row 0, col 1)
    kBrailleDot5,  // 右中上(row 1, col 1)
    kBrailleDot6,  // 右中下(row 2, col 1)
    kBrailleDot8,  // 右下  (row 3, col 1)
}};

// 子像素线性下标（col * 4 + row，先列后行）→ 点掩码。越界返回 0（不点亮）。
constexpr std::uint8_t braille_dots_for_cell(int col, int row) noexcept {{
    if (col < 0 || col > 1 || row < 0 || row > 3) {{
        return 0;
    }}
    return kBrailleCellDots[static_cast<std::size_t>(col) * 4 + static_cast<std::size_t>(row)];
}}

// 清空全部点（空白盲文）：U+2800。
inline constexpr char32_t kBrailleBlank = U'\\u2800';

}}  // namespace termchart::detail

#endif  // TERMCHART_DETAIL_BRAILLE_TABLE_HPP
"""


def render() -> str:
    lines = []
    for row in range(0, 256, 8):
        cells = ", ".join("U'\\u%04X'" % (0x2800 + v) for v in range(row, row + 8))
        lines.append("    " + cells + ",")
    lines[-1] = lines[-1].rstrip(",")
    return TEMPLATE.format(chars="\n".join(lines))


def main(argv: list[str]) -> int:
    changed = emit(OUT, render(), wants_check(argv))
    return 1 if changed and wants_check(argv) else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
