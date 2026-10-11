#!/usr/bin/env python3
"""
gen_turbo_table.py —— assets/colormaps/turbo.dat → include/termchart/detail/turbo_table.hpp

纯标准库（不依赖 matplotlib），因此可以进构建管线：`generate_all` 会跑它，
`check_generated` 会跑 `--check` 保证头文件与数据源永远一致。
数据源的来路与自检见 assets/colormaps/turbo.dat 的头部注释。
"""
from __future__ import annotations

import sys
from pathlib import Path

from _genutil import ROOT, emit, wants_check

SRC = ROOT / "assets" / "colormaps" / "turbo.dat"
OUT = ROOT / "include" / "termchart" / "detail" / "turbo_table.hpp"

EXPECT_COUNT = 256

TEMPLATE = """// turbo_table.hpp —— 由 tools/gen_turbo_table.py 生成，禁手改。
//
// 数据源：assets/colormaps/turbo.dat（官方 Turbo 色表，Google，Apache-2.0）。
// 生成器只做"数据搬运"，不参与任何数值加工——色值需要改就改 .dat 再重跑。
//
// 层号 -1：横切数据表，任何层都可 include 本文件；它不依赖 termchart 其余文件。
#ifndef TERMCHART_DETAIL_TURBO_TABLE_HPP
#define TERMCHART_DETAIL_TURBO_TABLE_HPP

#include <array>
#include <cstdint>

namespace termchart::detail {{

struct TurboEntry {{
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
}};

// 256 级，索引 0 = 最低值端（深紫），255 = 最高值端（暗红）。
// clang-format off
inline constexpr std::array<TurboEntry, {count}> kTurboTable{{{{
{entries}
}}}};
// clang-format on

}}  // namespace termchart::detail

#endif  // TERMCHART_DETAIL_TURBO_TABLE_HPP
"""


def read_table() -> list[tuple[int, int, int]]:
    rows: list[tuple[int, int, int]] = []
    for raw in SRC.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(",")
        if len(parts) != 3:
            raise SystemExit(f"turbo.dat 行格式错误（应为 r,g,b）：{line!r}")
        try:
            triple = tuple(int(p) for p in parts)
        except ValueError:
            raise SystemExit(f"turbo.dat 含非整数：{line!r}")
        for value in triple:
            if not 0 <= value <= 255:
                raise SystemExit(f"turbo.dat 色值越界（0..255）：{line!r}")
        rows.append(triple)  # type: ignore[arg-type]
    if len(rows) != EXPECT_COUNT:
        raise SystemExit(f"turbo.dat 应有 {EXPECT_COUNT} 行，实际 {len(rows)} 行")
    return rows


def render() -> str:
    rows = read_table()
    lines = []
    for i in range(0, len(rows), 4):
        chunk = ", ".join("{%3d, %3d, %3d}" % entry for entry in rows[i:i + 4])
        lines.append("    " + chunk + ",")
    lines[-1] = lines[-1].rstrip(",")
    return TEMPLATE.format(count=len(rows), entries="\n".join(lines))


def main(argv: list[str]) -> int:
    check = wants_check(argv)
    changed = emit(OUT, render(), check)
    return 1 if changed and check else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
