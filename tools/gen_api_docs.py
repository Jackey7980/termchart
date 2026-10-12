#!/usr/bin/env python3
"""gen_api_docs.py —— 生成 docs/reference/api.md（API 参考索引）。

扫描 include/termchart/** 的公开头文件，按模块（层号取自
cmake/TermchartModules.cmake 的 TERMCHART_MODULE_LAYERS）分节，每个头列出：
  * 头文件顶部注释（原样搬运，只去 `//` 前缀）；
  * class / struct / enum class 名称；
  * 命名空间级函数声明（含头内 inline 定义，取到签名为止）。

只搬运声明与注释，不添加手写说明。解析是逐字符的括号深度跟踪：
进入 class/struct/enum 体内或函数体后不再收集（类成员与函数局部声明不列），
进入 namespace 不影响收集。--check 模式不写文件，只比对（_genutil 约定）。

已知边界：按"; { }"切语句的正则级解析认不出宏里藏的声明与字符串里的括号，
本仓库公开头不使用这类写法；若将来有，属于本脚本要跟进的边界。
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

from _genutil import ROOT, emit, wants_check

INCLUDE_DIR = ROOT / "include" / "termchart"
MODULES_CMAKE = ROOT / "cmake" / "TermchartModules.cmake"
OUT = ROOT / "docs" / "reference" / "api.md"

TEMPLATE_RE = re.compile(r"^template\s*<")
CLASS_RE = re.compile(r"^(?:class|struct)\s+(\w+)")
ENUM_RE = re.compile(r"^enum\s+class\s+(\w+)")
FUNC_RE = re.compile(
    r"^(?!using\b|typedef\b|friend\b|static_assert\b|namespace\b|template\b)"
    r"[A-Za-z_][\w:<>,&*\s()\[\]~]*\b\w+\s*\([^)]*\)\s*(?:const)?\s*(?:noexcept\s*)?$"
)
NOISE_PREFIX = ("#", "using ", "typedef ", "friend ", "static_assert")


def module_layers() -> dict[str, int]:
    text = MODULES_CMAKE.read_text(encoding="utf-8")
    m = re.search(r"set\(TERMCHART_MODULE_LAYERS(.*?)\)", text, re.S)
    if not m:
        raise SystemExit("无法从 TermchartModules.cmake 解析层号表")
    layers: dict[str, int] = {}
    for name, num in re.findall(r'"(\w+)=(\d+)"', m.group(1)):
        layers[name] = int(num)
    return layers


def strip_line_comment(line: str) -> str:
    # 近似：公开头没有把 "//" 放进字符串字面量的写法。
    i = line.find("//")
    return line if i < 0 else line[:i]


def top_comment(lines: list[str]) -> list[str]:
    """头文件顶部的连续 `//` 注释（到第一个非注释行为止），去掉前缀。"""
    out: list[str] = []
    for line in lines:
        s = line.strip()
        if s.startswith("//"):
            out.append(s[2:].strip())
        elif s == "":
            if out:
                break
        else:
            break
    return out


def parse_header(path: Path) -> tuple[list[str], list[str]]:
    """返回（顶部注释行, 声明列表）。声明为整理后的单行字符串。"""
    lines = path.read_text(encoding="utf-8").splitlines()
    doc = top_comment(lines)

    decls: list[str] = []
    depth = 0
    # 与 depth 平行的抑制栈：scope_suppressed[i] 表示第 i+1 层作用域内
    # 是否不收集（类/枚举体、函数体、初始化列表）。栈保证内层 "} " 闭合
    # 只弹自己的那一层，不会误清外层类体的抑制。
    scope_suppressed: list[bool] = []
    stmt = ""

    def flush_stmt() -> None:
        nonlocal stmt
        s = " ".join(stmt.split())
        stmt = ""
        if scope_suppressed and scope_suppressed[-1]:
            return
        if not s:
            return
        if s.startswith(NOISE_PREFIX) or s.startswith("operator"):
            return
        if CLASS_RE.match(s) or ENUM_RE.match(s):
            decls.append(s)
            return
        if "(" in s and ")" in s and FUNC_RE.match(s):
            decls.append(s)

    for raw in lines:
        line = strip_line_comment(raw)
        if depth == 0 and stmt == "" and line.lstrip().startswith("#"):
            continue  # 预处理行
        enclosing_suppressed = bool(scope_suppressed) and scope_suppressed[-1]
        for ch in line:
            if ch == ";":
                flush_stmt()
            elif ch == "{":
                s = " ".join(stmt.split())
                stmt = ""
                if enclosing_suppressed:
                    sup = True
                elif CLASS_RE.match(s) or ENUM_RE.match(s):
                    decls.append(s)
                    sup = True  # 类/枚举体内不收集
                elif s.startswith("namespace"):
                    sup = False
                else:
                    sup = True  # 函数体 / 初始化列表等匿名作用域
                scope_suppressed.append(sup)
                depth += 1
            elif ch == "}":
                if scope_suppressed:
                    scope_suppressed.pop()
                depth -= 1
                enclosing_suppressed = bool(scope_suppressed) and scope_suppressed[-1]
            else:
                stmt += ch
    flush_stmt()
    return doc, decls


def collect() -> dict[str, list[tuple[str, list[str], list[str]]]]:
    """模块 → [(相对路径, 顶部注释, 声明)]，模块按层号排序。"""
    layers = module_layers()
    groups: dict[str, list[tuple[str, list[str], list[str]]]] = {}
    for path in sorted(INCLUDE_DIR.rglob("*.hpp")):
        rel = path.relative_to(INCLUDE_DIR).as_posix()
        parts = rel.split("/")
        module = parts[0] if len(parts) > 1 else "（顶层头）"
        doc, decls = parse_header(path)
        groups.setdefault(module, []).append((rel, doc, decls))
    # 模块排序：顶层头 → 按层号；未登记层号的模块排最后（按名字）
    def sort_key(mod: str) -> tuple[int, str]:
        if mod == "（顶层头）":
            return (0, "")
        layer = layers.get(mod)
        return (1, "") if layer is None else (2, f"{layer:02d}{mod}")

    return {m: groups[m] for m in sorted(groups, key=sort_key)}


def render(groups: dict[str, list[tuple[str, list[str], list[str]]]]) -> str:
    layers = module_layers()
    total = sum(len(v) for v in groups.values())
    lines = [
        "<!-- 由 tools/gen_api_docs.py 生成，禁手改。更新方式：重跑该脚本。 -->",
        "# API 参考",
        "",
        f"扫描 `include/termchart/**` 的 **{total} 个公开头文件**生成，按模块（层号源：",
        "`cmake/TermchartModules.cmake`）分节。每个头列出顶部注释与声明——",
        "class / struct / enum class / 命名空间级函数（含头内 inline 定义，签名原样搬运）。",
        "本文件不含手写说明；各声明语义见对应头文件。",
        "",
        "## 模块索引",
        "",
        "| 模块 | 层 | 头文件数 |",
        "| --- | --- | --- |",
    ]
    for mod, items in groups.items():
        layer = "—" if mod == "（顶层头）" else "L-1" if mod == "detail" else f"L{layers[mod]}" if mod in layers else "?"
        lines.append(f"| {mod} | {layer} | {len(items)} |")
    lines.append("")

    for mod, items in groups.items():
        if mod == "（顶层头）":
            label = "（顶层头）"
        elif mod == "detail":
            label = "detail（L-1，生成的数据表，禁手改）"
        elif mod in layers:
            label = f"{mod}（L{layers[mod]}）"
        else:
            label = f"{mod}（层号未登记）"
        lines.append(f"## {label}")
        lines.append("")
        for rel, doc, decls in items:
            lines.append(f"### `{rel}`")
            lines.append("")
            for d in doc:
                lines.append(f"> {d}".rstrip())
            if doc:
                lines.append("")
            if decls:
                for d in decls:
                    lines.append(f"- `{d}`")
            else:
                lines.append("- （无命名空间级声明，或为纯宏 / 伞形头）")
            lines.append("")
    return "\n".join(lines) + "\n"


def main(argv: list[str]) -> int:
    check = wants_check(argv)
    content = render(collect())
    changed = emit(OUT, content, check)
    return 1 if changed and check else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
