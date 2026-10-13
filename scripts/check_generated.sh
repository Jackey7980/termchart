#!/usr/bin/env bash
# check_generated.sh —— 生成物与生成器必须一致（无 diff）
#
# 这是 cmake 目标 `check_generated` 的 shell 封装（ROADMAP 阶段 0 未勾选项之一）。
# 清单必须与 `cmake/TermchartGenerate.cmake` 的 `check_generated` 目标**逐一对应**；
# 新增生成器时两处都要加，否则会出现"cmake 侧红、shell 侧绿"的假绿。
#
# 为什么必须有：CI 的 guardrails job 不装编译器，只装 Python，因此它跑不了 cmake
# 目标，只能跑脚本。护栏只在本地跑等于没有护栏。
#
# 用法：
#   scripts/check_generated.sh
#
# 退出码：任一生成物过期即 1。**此时只能重跑生成器，不得手改产物。**

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

python_bin="python3"
command -v "$python_bin" >/dev/null 2>&1 || python_bin="python"

status=0

run_check() {
    echo "── $1"
    if ! "$python_bin" "tools/$1" --check; then
        echo "   ↑ 过期：请跑 cmake --build <dir> --target generate_all（或 python tools/$1）" >&2
        status=1
    fi
}

# 顺序与 cmake/TermchartGenerate.cmake::check_generated 保持一致
run_check gen_unicode_tables.py
run_check gen_grapheme_tables.py
run_check gen_braille_table.py
run_check gen_turbo_table.py
run_check gen_dsl_samples.py
run_check gen_fixtures.py
run_check gen_module_graph.py
run_check gen_single_header.py

# 注意：gen_api_docs.py 在 cmake 侧也不进 check_generated（纯头扫描、体量大）；
# gen_gallery.py 依赖已构建的示例二进制，只能在"构建后"单独跑。两者都刻意不在这里。

if [ "$status" -ne 0 ]; then
    echo "生成物校验失败：产物与生成器不一致。" >&2
fi
exit "$status"
