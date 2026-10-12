#!/usr/bin/env python3
"""gen_single_header.py —— 生成 single_include/termchart.hpp（单头产物，阶段 7）。

算法（规格见 cmake/TermchartSingleHeader.cmake 注释）：
  1. 收集 include/termchart/ 下全部 .hpp（含顶层头与 detail/ 生成表）。
  2. 拓扑排序：解析每个头内部的 #include "termchart/..." 依赖边（这些边在产物里
     会被剥掉，但决定拼接顺序），Kahn 算法；就绪集按 (层号, 组别, 路径) 决胜 ——
     层号取 cmake/TermchartModules.cmake 的 TERMCHART_MODULE_LAYERS；组别：
     顶层头(0) < detail 生成表(1) < 普通模块头(2) < 伞形头(3)。
     伞形头放 core 组最后：它的 `namespace tc = termchart;` 别名要求
     namespace termchart 已被声明，纯字母序满足不了这条隐含依赖。
  3. 剥离：每个头自身的 include guard、#pragma once、全部内部 include；
     <...> 系统 include 按名去重（保序取首次出现；条件编译块内的不动，
     因为首现可能在互斥分支里，剥掉会破坏另一分支）。
  4. 产物整体套 TERMCHART_SINGLE_HEADER 守卫 + #pragma once，并在未定义时
     默认 #define TERMCHART_STATIC：单头用法必然链接静态库或自编译 src/，
     export.hpp 需要把 TERMCHART_API 展开为空。
  5. --check：重新生成并与现有产物比对，不一致退出码非零。

产物只拼头文件、不拼 src/**：每个头的独立可编译性由 check_headers 守住，
链接由使用方的构建提供。
"""
from __future__ import annotations

import heapq
import re
import sys
from collections import defaultdict
from pathlib import Path

from _genutil import ROOT, emit, wants_check

SRC = ROOT / "include" / "termchart"
OUT = ROOT / "single_include" / "termchart.hpp"

RE_INTERNAL = re.compile(r'^\s*#include\s+"termchart/[^"]+"\s*$')
RE_SYSTEM = re.compile(r"^\s*#include\s*<([^>]+)>\s*$")
RE_IF = re.compile(r"^\s*#\s*(?:if|ifdef|ifndef)\b")
RE_ENDIF = re.compile(r"^\s*#\s*endif\b")
RE_GUARD_IFNDEF = re.compile(r"^#ifndef\s+(TERMCHART_\w+)\s*$")
RE_GUARD_DEFINE = re.compile(r"^#define\s+(\S+)\s*$")
RE_ENDIF_TAIL = re.compile(r"^#endif\b")


def module_layers() -> dict[str, int]:
    text = (ROOT / "cmake" / "TermchartModules.cmake").read_text(encoding="utf-8")
    m = re.search(r"set\(TERMCHART_MODULE_LAYERS(.*?)\)", text, re.S)
    if not m:
        raise SystemExit("TermchartModules.cmake 里找不到 TERMCHART_MODULE_LAYERS")
    return {name: int(num) for name, num in re.findall(r'"(\w+)=(\d+)"', m.group(1))}


def group_key(rel: str, layers: dict[str, int]) -> tuple[int, int, str]:
    """拼接顺序的决胜键：(层号, 组别, 路径)。"""
    if "/" not in rel:
        # 顶层头；伞形头排 core 组最后（其 namespace 别名需要 termchart 先声明）
        return (0, 3 if rel == "termchart.hpp" else 0, rel)
    if rel.startswith("detail/"):
        return (0, 1, rel)  # detail 生成表视为 core 层，早于一切用表者
    module = rel.split("/", 1)[0]
    return (layers.get(module, 1 << 30), 2, rel)


def collect() -> dict[str, str]:
    """返回 {相对 include/termchart 的路径: 文件全文}。"""
    files: dict[str, str] = {}
    for p in sorted(SRC.rglob("*.hpp")):
        rel = p.relative_to(SRC).as_posix()
        files[rel] = p.read_text(encoding="utf-8")
    return files


def topo_order(files: dict[str, str], layers: dict[str, int]) -> list[str]:
    """Kahn 拓扑排序；就绪集按 group_key 决胜。"""
    edges: dict[str, set[str]] = {}
    indeg: dict[str, int] = {f: 0 for f in files}
    dependents: dict[str, list[str]] = defaultdict(list)
    for rel, text in files.items():
        deps = set(re.findall(r'#include\s+"termchart/([^"]+)"', text)) & files.keys()
        deps.discard(rel)
        edges[rel] = deps
        indeg[rel] = len(deps)
        for d in deps:
            dependents[d].append(rel)

    ready = [(group_key(f, layers), f) for f, n in indeg.items() if n == 0]
    heapq.heapify(ready)
    order: list[str] = []
    while ready:
        _, f = heapq.heappop(ready)
        order.append(f)
        for dep in dependents[f]:
            indeg[dep] -= 1
            if indeg[dep] == 0:
                heapq.heappush(ready, (group_key(dep, layers), dep))
    if len(order) != len(files):
        stuck = sorted(f for f, n in indeg.items() if n > 0)
        raise SystemExit(f"内部 include 存在循环依赖，无法拓扑排序：{stuck}")
    return order


def strip_own_guard(lines: list[str]) -> list[str]:
    """剥掉头文件自身的经典 include guard（#ifndef X / #define X / 末尾 #endif）。

    产物已有外层守卫，per-file guard 冗余；剥掉后条件深度从 0 起算，
    系统 include 去重的"无条件"判定才准确。校验不过就原样保留（安全降级）。
    """
    for i, ln in enumerate(lines):
        m = RE_GUARD_IFNDEF.match(ln)
        if not m:
            continue
        nxt = next((j for j in range(i + 1, len(lines)) if lines[j].strip()), None)
        if nxt is None or not RE_GUARD_DEFINE.match(lines[nxt]):
            return lines
        last = max(j for j in range(len(lines)) if lines[j].strip())
        if not RE_ENDIF_TAIL.match(lines[last].strip()):
            return lines
        return [l for j, l in enumerate(lines) if j not in (i, nxt, last)]
    return lines


def process(text: str, seen_system: set[str]) -> str:
    lines = strip_own_guard(text.splitlines())
    out: list[str] = []
    depth = 0
    for ln in lines:
        if RE_IF.match(ln):
            depth += 1
        elif RE_ENDIF.match(ln):
            depth = max(0, depth - 1)
        if ln.strip() == "#pragma once":
            continue
        if RE_INTERNAL.match(ln):
            continue  # 内部引用：全文已在此前拼入，剥掉恒安全
        ms = RE_SYSTEM.match(ln)
        if ms:
            # 仅去重"无条件"出现；条件块内的保留（首现可能在互斥分支里）
            if depth == 0:
                if ms.group(1) in seen_system:
                    continue
                seen_system.add(ms.group(1))
        out.append(ln.rstrip())
    while out and not out[-1]:
        out.pop()
    return "\n".join(out)


def build() -> tuple[str, int]:
    layers = module_layers()
    files = collect()
    order = topo_order(files, layers)
    seen_system: set[str] = set()
    parts: list[str] = []
    for rel in order:
        body = process(files[rel], seen_system)
        parts.append(f"\n// ===== include/termchart/{rel} =====\n{body}\n")
    head = (
        "// single_include/termchart.hpp —— 由 tools/gen_single_header.py 生成，禁手改。\n"
        f"//\n"
        f"// 由 {len(order)} 个头文件按依赖拓扑序拼接：层号取 cmake/TermchartModules.cmake，\n"
        "// 组别为 顶层头 → detail 生成表 → 普通模块头（伞形头殿后），同组按路径字母序决胜；\n"
        "// 内部 #include 已剥除，系统头按名去重（保序取首次出现）。\n"
        "// 重跑生成器：python tools/gen_single_header.py（--check 校验一致）。\n"
        "//\n"
        '// 用法：把本文件所在目录加入 include 路径后 #include "termchart.hpp"，\n'
        "// 再链接 termchart 静态库（或自行编译 src/ 下同名实现文件）。\n"
        "#ifndef TERMCHART_SINGLE_HEADER\n"
        "#define TERMCHART_SINGLE_HEADER\n"
        "#pragma once\n"
        "\n"
        "// 单头用法必然链接静态库/自编译源码：未显式定义时默认补上，\n"
        "// 使 export.hpp 把 TERMCHART_API 展开为空。\n"
        "#ifndef TERMCHART_STATIC\n"
        "#define TERMCHART_STATIC\n"
        "#endif\n"
    )
    content = head + "".join(parts) + "\n#endif  // TERMCHART_SINGLE_HEADER\n"
    return content, len(order)


def main(argv: list[str]) -> int:
    check = wants_check(argv)
    content, count = build()
    changed = emit(OUT, content, check)
    if not check:
        print(f"拼入头文件：{count} 个；产物 {content.count(chr(10))} 行")
    return 1 if changed and check else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
