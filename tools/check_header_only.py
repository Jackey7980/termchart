#!/usr/bin/env python3
"""
check_header_only.py —— 头文件自洽性校验（物理布局约束）

对每个 include/termchart/**.hpp：
  - header-only 清单内的文件：不得存在同名 src/*.cpp；
  - 其余文件：必须存在同名 src/*.cpp（基类 cpp 放虚表锚点，也算实现单元）。

对 src/**.cpp：不得出现清单外的孤儿实现（即既无 include/ 公开头、也无
src/ 同名私有头的 cpp）。三类合法例外：
  - **私有实现单元**：存在同名 src/xxx.hpp 声明表但不对应 include/ 公开头，
    例如 image/codec/hevc/*.cpp、image/codec/bitstream/*.cpp。自动判定，
    无需逐条登记。
  - **平台拆分实现**：src/term/terminal_posix.cpp ↔ term/terminal.hpp
    （见 PLATFORM_SUFFIXES）。
  - **无私有头的特殊实现**：见 SRC_INTERNAL_CXX。

两类天然无需实现单元的头文件同样豁免：
  - detail/*：由 tools/gen_*.py 产出的横切数据表，纯 constexpr 数据；
  - 顶层文件（version/export/deprecate/fwd）：只有宏、常量与前向声明。

退出码：0 通过，1 违规。
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INCLUDE_ROOT = ROOT / "include" / "termchart"
SRC_ROOT = ROOT / "src"
HEADER_ONLY_TXT = ROOT / "cmake" / "header_only.txt"

# 平台拆分实现的后缀：src/term/terminal_posix.cpp ↔ include/termchart/term/terminal.hpp
PLATFORM_SUFFIXES = ("_posix", "_win32", "_darwin", "_linux", "_macos")

# 无同名 src/xxx.hpp 私有头的特殊实现单元（有私有头的由 main() 自动豁免）。
# network/http_parse 与 network/dns_cache 有同名私有头，已由自动判定覆盖；
# 下列 proxy_* / wasm 则没有同名头，需要显式登记：
# proxy_* 四件套的声明集中在 proxy_internal.hpp（公开面只有 http.hpp 的
# HttpOptions::proxy）；wasm/termchart_wasm 是 Emscripten 专用入口，仅为 WASM
# 运行时暴露 C 出口，无公开头也无私有头。
SRC_INTERNAL_CXX = (
    "network/proxy_detect",
    "network/proxy_tunnel",
    "network/proxy_sys_win32",
    "network/proxy_sys_posix",
    # Emscripten 专用入口 TU：仅 __EMSCRIPTEN__ 下编译（顶层 CMakeLists 用
    # if(EMSCRIPTEN AND TERMCHART_BUILD_WASM) 门控），桌面/CI 构建不碰；无公开头文件，
    # 仅为 WASM 运行时暴露 C 出口（termchart_blit_canvas / termchart_render_demo）。
    "wasm/termchart_wasm",
)


def read_header_only() -> set[str]:
    """读 cmake/header_only.txt。清单里带不带 .hpp 后缀都接受，统一去后缀。"""
    items: set[str] = set()
    for raw in HEADER_ONLY_TXT.read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        items.add(Path(line).with_suffix("").as_posix())
    return items


def is_exempt_header(header: str) -> bool:
    """豁免：生成的数据表与顶层宏/前向声明头不需要实现单元。"""
    return header.startswith("detail/") or "/" not in header


def platform_parent(cpp: str) -> str | None:
    """平台拆分实现的母头路径；不是拆分实现则返回 None。"""
    stem = Path(cpp).as_posix()
    for suffix in PLATFORM_SUFFIXES:
        if stem.endswith(suffix):
            return stem[: -len(suffix)]
    return None


def main() -> int:
    header_only = read_header_only()
    violations: list[str] = []

    headers = sorted(p.relative_to(INCLUDE_ROOT).with_suffix("").as_posix()
                     for p in INCLUDE_ROOT.rglob("*.hpp"))
    cpps = sorted(p.relative_to(SRC_ROOT).with_suffix("").as_posix()
                  for p in SRC_ROOT.rglob("*.cpp"))

    header_set = set(headers)
    cpp_set = set(cpps)

    for h in headers:
        if h in header_only and h in cpp_set:
            violations.append(f"header-only 文件不应有 src 实现：src/{h}.cpp")
        if h not in header_only and h not in cpp_set and not is_exempt_header(h):
            violations.append(f"缺失同名实现：src/{h}.cpp")

    for c in cpps:
        if c in header_set:
            continue
        if c in SRC_INTERNAL_CXX:
            continue
        # 私有实现单元：存在同名 src/xxx.hpp 声明表，只是不对应 include/
        # 公开头。image/codec/** 下的 HEVC/H.264 编解码 TU 全属此类，
        # 自动判定避免每新增一个 codec 文件都要回来补白名单。
        if (SRC_ROOT / f"{c}.hpp").is_file():
            continue
        parent = platform_parent(c)
        if parent is not None and parent in header_set:
            continue
        violations.append(f"孤儿实现（无对应头文件）：src/{c}.cpp")

    if violations:
        print("头文件自洽性校验失败：", file=sys.stderr)
        for v in violations:
            print(f"  {v}", file=sys.stderr)
        return 1

    print(f"头文件自洽性校验通过（{len(headers)} 头 / {len(cpps)} 实现）。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
