#!/usr/bin/env python3
"""
gen_turbo_dat.py —— 抽取官方 Turbo 色表 → assets/colormaps/turbo.dat

为什么需要它：Turbo 没有解析式定义（Google 发布的就是一张 256 级查找表），
本库原先用"锚点插值"凑形状、多项式系数又是坏的，颜色与官方表不符。
这里从 **matplotlib 的 `turbo` listed colormap** 取回官方数据（matplotlib 直接
采用 Google 发布的同一张表），落成仓库内的唯一数据源。

为什么**不**接进 generate_all / check_generated：它依赖第三方包 matplotlib，
而本仓库的构建管线必须"零外部依赖可用"（生成器只用标准库）。因此：
  * 正常路径：`assets/colormaps/turbo.dat` 已提交入库，构建只跑
    `gen_turbo_table.py`（纯标准库）把 .dat 变成 detail 头；
  * 审计路径：本脚本 `--check` 用来确认入库的 .dat 仍与 matplotlib 的官方表一致。

用法（任一带 matplotlib 的解释器）：
    python tools/gen_turbo_dat.py            # 写文件
    python tools/gen_turbo_dat.py --check    # 只校验，不改文件
"""
from __future__ import annotations

import sys
from pathlib import Path

from _genutil import ROOT, emit, wants_check

OUT = ROOT / "assets" / "colormaps" / "turbo.dat"

# 官方文档给出的首末色（乘 255 后即下表首末项），用作取数自检。
EXPECT_FIRST = (48, 18, 59)
EXPECT_LAST = (122, 4, 3)

HEADER = """# turbo.dat —— 官方 Turbo 色表（256 级，RGB 0..255）
#
# 来源：Google 的 Turbo 色表（Anton Mikhailov / Google LLC，Apache License 2.0）。
# 取数方式：matplotlib 的 listed colormap `turbo`（matplotlib 直接采用该官方表），
#           由 tools/gen_turbo_dat.py 抽出；原始值为 0..1 浮点，四舍五入到 8 位整数
#           （本库的 Rgb 就是 uint8）。
# 自检：第 0 级 = (48,18,59)、第 255 级 = (122,4,3)，与官方文档给出的首末色一致。
#
# 本文件是**唯一数据源**：include/termchart/detail/turbo_table.hpp 由
# tools/gen_turbo_table.py 从它生成（生成物禁手改）。改动本文件即等同于换色表，
# 需要同时跑 generate_turbo_table 并更新 golden 快照。
#
# 格式：每行 r,g,b（十进制 0..255），共 256 行，按级次升序。
"""


def sample_table() -> list[tuple[int, int, int]]:
    try:
        import matplotlib
    except ImportError:  # pragma: no cover - 审计路径才会用到
        raise SystemExit(
            "需要 matplotlib 才能抽取官方表；请用带 matplotlib 的解释器运行本脚本。"
        )
    colors = list(matplotlib.colormaps["turbo"].colors)
    if len(colors) != 256:
        raise SystemExit(f"matplotlib 的 turbo 级数不是 256，而是 {len(colors)}")
    rows = [tuple(int(round(channel * 255)) for channel in entry[:3]) for entry in colors]
    if rows[0] != EXPECT_FIRST or rows[-1] != EXPECT_LAST:
        raise SystemExit(
            f"取数自检失败：首={rows[0]} 末={rows[-1]}，"
            f"期望首={EXPECT_FIRST} 末={EXPECT_LAST}（可能是别的色表或版本变了）"
        )
    return rows


def render() -> str:
    rows = sample_table()
    return HEADER + "\n" + "\n".join(f"{r},{g},{b}" for r, g, b in rows) + "\n"


def main(argv: list[str]) -> int:
    check = wants_check(argv)
    changed = emit(OUT, render(), check)
    return 1 if changed and check else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
