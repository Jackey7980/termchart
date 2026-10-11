#!/usr/bin/env python3
"""
check_layers.py —— 分层规则校验

读取 cmake/TermchartModules.cmake，扫描 include/termchart/ 下全部头文件，
按 FORBIDDEN_EDGES > NARROW_EDGES > 层号 的顺序判定每条 #include。

退出码：0 通过，1 存在违规。
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INCLUDE_DIR = ROOT / "include" / "termchart"
CMAKE_FILE = ROOT / "cmake" / "TermchartModules.cmake"

# 伞形头豁免层号判定（它们是聚合入口，天然依赖全模块）。
EXEMPT_FILES = {"termchart.hpp", "termchart_full.hpp"}

# 匹配 #include "termchart/..." 或 #include <termchart/...>
INCLUDE_RE = re.compile(r'#\s*include\s+[<"](termchart/[^">]+)[">]')


def parse_layers(text: str) -> dict[str, int]:
    m = re.search(r"set\(TERMCHART_MODULE_LAYERS(.*?)\n\)", text, re.S)
    if not m:
        raise SystemExit("无法从 cmake 解析 TERMCHART_MODULE_LAYERS")
    layers: dict[str, int] = {}
    for entry in re.findall(r'"([^"]+)"', m.group(1)):
        name, num = entry.split("=")
        layers[name.strip()] = int(num)
    return layers


def parse_edge_set(text: str, var: str) -> set[tuple[str, str]]:
    m = re.search(r"set\(" + var + r"(.*?)\n\)", text, re.S)
    if not m:
        return set()
    body = re.sub(r"#.*", "", m.group(1))  # 去行内注释
    pairs: set[tuple[str, str]] = set()
    for item in re.findall(r'"([^"]+)"', body):
        left, right = item.split(">")
        pairs.add((left.strip(), right.strip()))
    return pairs


def module_of(rel: str) -> str | None:
    """相对 include/termchart/ 的去扩展名路径 → 模块名。顶层返回 None。"""
    return rel.split("/", 1)[0] if "/" in rel else None


def layer_of(rel: str, layers: dict[str, int]) -> int:
    """相对 include/termchart/ 的去扩展名路径 → 层号，永远返回 int。
    顶层工具头 → 0；detail/ 横切目录 → -1（低于 core，任何层可 include）；
    其余模块必须在 LAYER 表中，不在即 KeyError。
    """
    mod = module_of(rel)
    if mod is None:
        return 0          # version/export/deprecate/fwd
    if mod == "detail":
        return -1         # 横切数据表，低于 core
    return layers[mod]    # 不在表中：KeyError


def main() -> int:
    text = CMAKE_FILE.read_text(encoding="utf-8")
    layers = parse_layers(text)
    narrow = parse_edge_set(text, "TERMCHART_NARROW_EDGES")
    forbidden = parse_edge_set(text, "TERMCHART_FORBIDDEN_EDGES")

    # 被收窄的模块对：{ (src_mod, dst_mod) }，命中则该对转白名单制
    narrowed_pairs = {(l.split("/")[0], r.split("/")[0]) for l, r in narrow}
    narrow_allowed = set(narrow)

    violations: list[str] = []

    for header in sorted(INCLUDE_DIR.rglob("*.hpp")):
        rel = header.relative_to(INCLUDE_DIR).with_suffix("").as_posix()
        if header.name in EXEMPT_FILES:
            continue

        for line_no, line in enumerate(header.read_text(encoding="utf-8").splitlines(), 1):
            m = INCLUDE_RE.search(line)
            if not m:
                continue
            dst_rel = Path(m.group(1)).with_suffix("").as_posix()
            dst_rel = dst_rel.removeprefix("termchart/")

            src_mod = module_of(rel)
            dst_mod = module_of(dst_rel)
            src_l = layer_of(rel, layers)
            dst_l = layer_of(dst_rel, layers)

            where = f"{header.relative_to(ROOT)}:{line_no}"

            # 1. 禁止边
            if (src_mod, dst_mod) in forbidden:
                violations.append(f"{where}: 禁止边 {src_mod}>{dst_mod}")
                continue

            # 2. 收窄边
            if (src_mod, dst_mod) in narrowed_pairs:
                if (rel, dst_rel) not in narrow_allowed:
                    violations.append(
                        f"{where}: 模块对 {src_mod}>{dst_mod} 已收窄，"
                        f"只允许白名单文件对，拒绝 {rel}>{dst_rel}"
                    )
                continue

            # 3. 层号
            if dst_l > src_l:
                violations.append(
                    f"{where}: 层号违规 {src_mod}({src_l}) → {dst_mod}({dst_l})"
                )

    if violations:
        print("分层校验失败：", file=sys.stderr)
        for v in violations:
            print(f"  {v}", file=sys.stderr)
        return 1

    print("分层校验通过。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
