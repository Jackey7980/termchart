#!/usr/bin/env python3
"""
check_headers.py —— 头文件自包含性校验：每个公开头都能**单独**编译通过

为什么必须有这一条：头文件一旦只能"被别人先 include 才能编过"，
就等于把依赖藏在了包含顺序里。终端栈如此，将来的 GUI 平行栈（termchart_gui
复用 scene 及以下）更是如此——那边每个头都会被独立 include 到自己的
widget / layout 里，隐式依赖会立刻变成编译错误。

做法：为 include/termchart/ 下每个 .hpp 生成一个只含该头的一行式 TU，
用 -fsyntax-only 编译；任一失败即退出码 1。

用法：
    python tools/check_headers.py [--compiler CXX] [--include DIR] [--jobs N]
默认编译器取环境变量 CXX，回退 "c++"；默认 include 目录是仓库的 include/。
"""
from __future__ import annotations

import concurrent.futures
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INCLUDE_ROOT = ROOT / "include"


def parse_args(argv: list[str]) -> tuple[str, str, int]:
    compiler = os.environ.get("CXX", "c++")
    include_dir = str(INCLUDE_ROOT)
    jobs = 8
    rest = list(argv)
    while rest:
        flag = rest.pop(0)
        if flag == "--compiler" and rest:
            compiler = rest.pop(0)
        elif flag == "--include" and rest:
            include_dir = rest.pop(0)
        elif flag == "--jobs" and rest:
            jobs = int(rest.pop(0))
        else:
            raise SystemExit(f"未知参数：{flag}")
    return compiler, include_dir, jobs


def compile_one(workdir: str, compiler: str, include_dir: str, rel: str) -> tuple[str, str]:
    tu = Path(workdir) / (rel.replace("/", "_").replace(".", "_") + ".cpp")
    tu.write_text(f'#include "{rel}"\nint main() {{ return 0; }}\n', encoding="utf-8")
    result = subprocess.run(
        [
            compiler,
            "-std=c++20",
            "-fsyntax-only",
            # TERMCHART_STATIC：静态使用时 TERMCHART_API 展开为空；缺了它，
            # 头文件里带 TERMCHART_API 的内联定义会被当成 dllimport 而报错。
            "-DTERMCHART_STATIC",
            "-I",
            include_dir,
            str(tu),
        ],
        capture_output=True,
        text=True,
        errors="replace",
    )
    return rel, ("" if result.returncode == 0 else result.stderr.strip())


def main(argv: list[str]) -> int:
    compiler, include_dir, jobs = parse_args(argv)
    headers = sorted(
        p.relative_to(INCLUDE_ROOT).as_posix() for p in INCLUDE_ROOT.rglob("*.hpp")
    )
    if not headers:
        print("没有找到任何头文件", file=sys.stderr)
        return 1

    failures: list[tuple[str, str]] = []
    with tempfile.TemporaryDirectory() as workdir:
        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
            tasks = [
                pool.submit(compile_one, workdir, compiler, include_dir, rel)
                for rel in headers
            ]
            for task in concurrent.futures.as_completed(tasks):
                rel, error = task.result()
                if error:
                    failures.append((rel, error))

    if failures:
        print(f"头文件自包含性校验失败（{len(failures)}/{len(headers)}）：", file=sys.stderr)
        for rel, error in sorted(failures):
            print(f"\n  include/{rel}", file=sys.stderr)
            for line in error.splitlines()[:8]:
                print(f"    {line}", file=sys.stderr)
        return 1

    print(f"头文件自包含性校验通过（{len(headers)} 头独立编译）。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
