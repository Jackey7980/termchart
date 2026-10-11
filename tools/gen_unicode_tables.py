#!/usr/bin/env python3
"""gen_unicode_tables.py —— 生成 detail/ 下三张横切数据表（层号 -1）。

  unicode_tables.hpp  East Asian Width / 组合记号 / 格式字符 / 控制 → 朴素宽度
  box_chars.hpp       邻接掩码 × 线宽 → 制表符码点（U+2500 系唯一来源）
  block_chars.hpp     块元素：下沿 / 左侧 / 半块 / 阴影 / 密度阶梯

数据在 _unicode_data.py，模板在 _unicode_templates.py。
--check 模式由 scripts/check_generated.sh 调用。
"""
from __future__ import annotations

import sys

from _genutil import ROOT, emit, wants_check
from _unicode_data import COMBINING, DOUBLE, EA_AMBIGUOUS, EA_WIDE, FORMAT, HEAVY, LIGHT, ROUND
from _unicode_templates import render_block_chars, render_box_chars, render_unicode_tables

DETAIL = ROOT / "include" / "termchart" / "detail"


def rng(name: str, ranges: list[tuple[int, int]]) -> str:
    rows = ",\n".join("    {0x%04X, 0x%04X}" % r for r in ranges)
    return (f"inline constexpr std::size_t {name}Count = {len(ranges)};\n"
            "// clang-format off\n"
            f"inline constexpr std::array<CpRange, {name}Count> {name}{{{{\n{rows},\n}}}};\n"
            "// clang-format on")


def main(argv: list[str]) -> int:
    check = wants_check(argv)
    changed = False
    changed |= emit(DETAIL / "unicode_tables.hpp", render_unicode_tables(
        wide=rng("kEastAsianWide", EA_WIDE),
        ambiguous=rng("kEastAsianAmbiguous", EA_AMBIGUOUS),
        combining=rng("kCombining", COMBINING),
        fmt=rng("kFormat", FORMAT),
    ), check)
    changed |= emit(DETAIL / "box_chars.hpp", render_box_chars(
        light=LIGHT, heavy=HEAVY, double=DOUBLE, round_=ROUND), check)
    changed |= emit(DETAIL / "block_chars.hpp", render_block_chars(), check)
    return 1 if changed and check else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
