#!/usr/bin/env python3
"""
_genutil.py —— 生成器公用出写助手（tools/ 内部使用，不是可执行脚本）。

约定：
  - 生成器负责"算出最终字节串"，不得拼接手写产物的内容；
  - 正常模式：写文件；
  - --check 模式：不写文件，比较现有内容，不一致则输出差异并返回非零退出码；
  - check_generated.sh 会对全部生成器跑 --check，保证产物（detail 数据表、
    附一/附四、module_matrix.md）与生成器永远一致。
"""
from __future__ import annotations

import difflib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def emit(path: Path, content: str, check: bool) -> bool:
    """写文件，或在校验模式下比较。返回 True 表示内容有变化（或文件缺失）。"""
    rel = path.relative_to(ROOT).as_posix()
    if not check:
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.exists() and path.read_text(encoding="utf-8") == content:
            print(f"无变化：{rel}")
            return False
        path.write_text(content, encoding="utf-8")
        print(f"已生成：{rel}")
        return True

    if not path.exists():
        print(f"[缺少生成物] {rel}", file=sys.stderr)
        return True
    existing = path.read_text(encoding="utf-8")
    if existing == content:
        print(f"一致：{rel}")
        return False
    diff = difflib.unified_diff(
        existing.splitlines(keepends=True),
        content.splitlines(keepends=True),
        fromfile=f"{rel} (仓库内)",
        tofile=f"{rel} (重新生成)",
        n=2,
    )
    print(f"[生成物过期] {rel}\n" + "".join(list(diff)[:80]), file=sys.stderr)
    return True


def wants_check(argv: list[str]) -> bool:
    return "--check" in argv
