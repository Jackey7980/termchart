#!/usr/bin/env python3
"""
gen_module_graph.py —— 读 cmake/TermchartModules.cmake，生成：

  1. 结构.md 的「附一、模块层号表」（原地幂等替换）
  2. docs/reference/module_matrix.md（整篇覆盖）
  3. 结构.md 的「附四、header-only 清单」（原地幂等替换，读 cmake/header_only.txt）

产物禁手改：改了 cmake 或本脚本后必须重跑，并由 check_generated.sh 校验无 diff。
"""
from __future__ import annotations

import re
import sys

from _genutil import ROOT, emit, wants_check

CMAKE_FILE = ROOT / "cmake" / "TermchartModules.cmake"
HEADER_ONLY_TXT = ROOT / "cmake" / "header_only.txt"
SPEC = ROOT / "结构.md"
MATRIX = ROOT / "docs" / "reference" / "module_matrix.md"

# ── 附一 ────────────────────────────────────────────────────────────────
APPENDIX1_HEAD = "### 附一、模块层号表（由 TermchartModules.cmake 生成，禁手改）"
APPENDIX1_TAIL = "判定顺序：禁止边（附三）→ 收窄边（附二）→ 层号（本表）。"

# ── 附四 ────────────────────────────────────────────────────────────────
APPENDIX4_HEAD = "### 附四、header-only 清单（由 header_only.txt 生成，禁手改）"
APPENDIX4_TAIL = ""


def parse_layers() -> list[tuple[str, int]]:
    text = CMAKE_FILE.read_text(encoding="utf-8")
    m = re.search(r"set\(TERMCHART_MODULE_LAYERS(.*?)\n\)", text, re.S)
    if not m:
        raise SystemExit("无法解析 TERMCHART_MODULE_LAYERS")
    out: list[tuple[str, int]] = []
    for entry in re.findall(r'"([^"]+)"', m.group(1)):
        name, num = entry.split("=")
        out.append((name.strip(), int(num)))
    out.sort(key=lambda kv: kv[1])
    return out


def render_table(layers: list[tuple[str, int]]) -> str:
    rows = ["| 层号 | 模块 | 允许 include（层号更低的全部模块） |",
            "| --- | --- | --- |"]
    for name, num in layers:
        allowed = "—" if num == 0 else f"≤{num - 1}"
        rows.append(f"| {num} | {name} | {allowed} |")
    return "\n".join(rows)


def render_appendix1(layers: list[tuple[str, int]]) -> str:
    return f"{APPENDIX1_HEAD}\n\n{render_table(layers)}\n\n{APPENDIX1_TAIL}\n"


def render_matrix(layers: list[tuple[str, int]]) -> str:
    return (
        "# 模块依赖矩阵\n\n"
        "> **本文件由 `tools/gen_module_graph.py` 从 `cmake/TermchartModules.cmake` 生成，禁手改。**\n"
        "> 内容同 `结构.md` 附一。\n\n"
        f"{render_table(layers)}\n\n"
        "判定顺序：禁止边（`结构.md` 附三）→ 收窄边（`结构.md` 附二）→ 层号（本表）。\n"
        "由 `tools/check_layers.py` 逐条 `#include` 执行。\n"
    )


def parse_header_only() -> list[str]:
    items: list[str] = []
    for raw in HEADER_ONLY_TXT.read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].strip()
        if line:
            items.append(line)
    return sorted(items)


def layer_numbers() -> dict[str, int]:
    return {name: num for name, num in parse_layers()}


def render_appendix4() -> str:
    layers = layer_numbers()
    buckets: dict[int, list[str]] = {}
    for item in parse_header_only():
        mod = item.split("/", 1)[0]
        buckets.setdefault(layers.get(mod, 0), []).append(item)
    rows = ["| 层 | 文件 |", "| --- | --- |"]
    for level in sorted(buckets):
        files = " ".join(sorted(buckets[level]))
        rows.append(f"| L{level} | {files} |")
    return f"{APPENDIX4_HEAD}\n\n" + "\n".join(rows) + "\n"


def replace_section(text: str, head: str, tail: str, new_block: str) -> str:
    """把 head..tail（含 tail 所在行）替换为 new_block，保持幂等。"""
    start = text.find(head)
    if start < 0:
        raise SystemExit(f"未找到待生成的段落：{head}")
    if tail:
        stopped = text.find(tail, start)
        if stopped < 0:
            raise SystemExit(f"未找到段落结束标记：{tail}")
        end = stopped + len(tail)
    else:
        # 无结束标记：吃到下一个 "### " 标题或文件尾
        nxt = text.find("\n### ", start + 1)
        end = len(text) if nxt < 0 else nxt + 1
    return text[:start] + new_block.rstrip("\n") + "\n" + text[end:].lstrip("\n")


def main(argv: list[str]) -> int:
    layers = parse_layers()
    check = wants_check(argv)

    spec_text = SPEC.read_text(encoding="utf-8")
    spec_text = replace_section(spec_text, APPENDIX1_HEAD, APPENDIX1_TAIL, render_appendix1(layers))
    spec_text = replace_section(spec_text, APPENDIX4_HEAD, APPENDIX4_TAIL, render_appendix4())

    # --check 模式不写文件，只报"生成物与生成器是否一致"（check_generated 依赖它）。
    changed_spec = emit(SPEC, spec_text, check)
    changed_matrix = emit(MATRIX, render_matrix(layers), check)
    return 1 if check and (changed_spec or changed_matrix) else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
