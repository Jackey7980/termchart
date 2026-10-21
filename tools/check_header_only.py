#!/usr/bin/env python3
"""
check_header_only.py —— 头文件自洽性校验（物理布局约束）

对每个 include/termchart/**.hpp：
  - header-only 清单内的文件：不得存在同名 src/*.cpp；
  - 其余文件：必须存在同名 src/*.cpp（基类 cpp 放虚表锚点，也算实现单元）。

对 src/**.cpp：不得出现清单外的孤儿实现（即有 cpp 无对应头文件），
但**平台拆分实现**例外：src/term/terminal_posix.cpp 是 term/terminal.hpp 的
POSIX 分支，母头存在即可（见 PLATFORM_SUFFIXES）。

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

# src/ 内部私有实现单元（有 src/xxx.hpp 私有声明表、不对应 include/ 公开头的
# .cpp）。先例：src/term/platform_internal.hpp 系列；network/http_parse 是
# 同类——把 HTTP 线格式解析做成纯函数 TU，测试直接 include 私有头咬分支；
# network/dns_cache 同理：进程级 TTL 缓存是对 connect 路径的透明优化，
# 不属于公开 API 面（公开面只有 socket.hpp 的选项与行为）。
# network/proxy_* 四件套：代理探测链与隧道协议（私有头 proxy_internal.hpp
# 声明，公开面只有 http.hpp 的 HttpOptions::proxy）。
SRC_INTERNAL_CXX = (
    "network/http_parse",
    "network/dns_cache",
    "network/proxy_detect",
    "network/proxy_tunnel",
    "network/proxy_sys_win32",
    "network/proxy_sys_posix",
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
