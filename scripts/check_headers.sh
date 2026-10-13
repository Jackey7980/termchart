#!/usr/bin/env bash
# check_headers.sh —— 每个公开头必须能独立编译（自包含性）
#
# 这是 cmake 目标 `check_headers` 的 shell 封装（ROADMAP 阶段 0 未勾选项之一）。
# 二者判据完全一致，区别只在入口：CI 里用 cmake 目标，本地/无 cmake 时用这个。
#
# 为什么必须有：GUI 平行栈会把 `include/termchart/**` 的单个头 include 进自己的
# widget，一个头"只有在被别人先 include 了才能编译"就是漏出去的耦合。
#
# 用法：
#   scripts/check_headers.sh                       # 自动探测编译器
#   scripts/check_headers.sh --compiler g++-14     # 指定编译器
#   CXX=/usr/bin/clang++ scripts/check_headers.sh  # 用环境变量也行
#
# 退出码：任一头失败即 1。

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

compiler="${CXX:-}"
if [ "$#" -gt 0 ]; then
    if [ "$1" = "--compiler" ] && [ "$#" -ge 2 ]; then
        compiler="$2"
    else
        echo "用法: $0 [--compiler <CXX>]" >&2
        exit 2
    fi
fi

if [ -z "$compiler" ]; then
    for c in "$CXX" c++ g++ clang++; do
        if command -v "$c" >/dev/null 2>&1; then
            compiler="$c"
            break
        fi
    done
fi

if [ -z "$compiler" ]; then
    echo "找不到 C++ 编译器：请用 --compiler 指定，或设置 CXX。" >&2
    exit 2
fi

python_bin="python3"
command -v "$python_bin" >/dev/null 2>&1 || python_bin="python"

exec "$python_bin" tools/check_headers.py \
    --compiler "$compiler" \
    --include "$repo_root/include"
