#!/usr/bin/env python3
"""
gen_golden.py —— 重新生成 tests/golden/ 下的属性快照

快照由测试二进制**自己写**：置 TERMCHART_UPDATE_GOLDEN=1 后跑 golden 用例，
golden_runner::check_or_update 会改为写入 tests/golden/<subdir>/<name>.txt。
本脚本只负责三件事：找到测试二进制、设好环境变量、在仓库根运行
（golden_root() 用相对路径 tests/golden，因此 cwd 必须是仓库根）。

用法：
    python tools/gen_golden.py                 # 自动在 build/*/tests/ 下找
    python tools/gen_golden.py <二进制或构建目录>

快照是契约：改动渲染输出后先跑测试确认确需更新，再跑本脚本；禁止手改快照。
"""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def binary_names() -> list[str]:
    return ["termchart_tests.exe", "termchart_tests"]


def find_tests_binary(arg: str | None) -> Path:
    if arg:
        given = Path(arg)
        if given.is_file():
            return given
        for name in binary_names():
            candidate = given / "tests" / name
            if candidate.is_file():
                return candidate
        raise SystemExit(f"指定路径下没有测试二进制：{given}")

    for tests_dir in sorted((ROOT / "build").glob("*/tests")):
        for name in binary_names():
            candidate = tests_dir / name
            if candidate.is_file():
                return candidate
    raise SystemExit("找不到 termchart_tests：先构建测试，或用第一个参数指定路径")


def main(argv: list[str]) -> int:
    binary = find_tests_binary(argv[0] if argv else None)
    env = dict(os.environ, TERMCHART_UPDATE_GOLDEN="1")
    print(f"生成 golden 快照：{binary.relative_to(ROOT).as_posix()}")
    completed = subprocess.run([str(binary), "--test-case=*golden*"],
                               cwd=str(ROOT), env=env, check=False)
    if completed.returncode != 0:
        print("golden 用例失败，快照未更新", file=sys.stderr)
        return completed.returncode
    for snapshot in sorted((ROOT / "tests" / "golden").rglob("*.txt")):
        print(f"  已写入 {snapshot.relative_to(ROOT).as_posix()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
