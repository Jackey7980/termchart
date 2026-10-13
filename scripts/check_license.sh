#!/usr/bin/env bash
# check_license.sh —— 许可证合规的最小快照检查
#
# 这是 ROADMAP 阶段 0 未勾选项之一。本仓是 Apache-2.0（`LICENSES/Apache-2.0.txt`），
# 且**刻意不给每个源文件加 SPDX 头**（310 个头文件逐个加头只会产生噪声，
# 许可证信息以仓库根目录为准）——所以这个脚本不做"每个文件都要有头"的检查。
#
# 它查的是三件真正会出问题的事：
#   1. 许可证文件本身存在且非空（改名/误删会静默丢掉合规信息）；
#   2. vendored 第三方（third_party/doctest）保留自己的许可证声明——删掉它
#      等于把别人的代码当自己的重新授权；
#   3. 自有源码里没有出现与 Apache-2.0 不相容的许可证字样（GPL 家族）。
#
# 用法：scripts/check_license.sh
# 退出码：任一检查失败即 1。

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

status=0
fail() {
    echo "  ✗ $1" >&2
    status=1
}
ok() {
    echo "  ✓ $1"
}

echo "── 1/3 许可证文件"
license="LICENSES/Apache-2.0.txt"
if [ ! -s "$license" ]; then
    fail "$license 缺失或为空"
elif grep -q "Apache License" "$license"; then
    ok "$license 存在且为 Apache 许可证全文"
else
    fail "$license 内容不像 Apache 许可证全文"
fi

echo "── 2/3 vendored 第三方保留原许可证"
# doctest 是唯一 vendored 依赖（MIT），检查它的版权声明还在。
doctest="third_party/doctest/doctest.h"
if [ ! -f "$doctest" ]; then
    fail "$doctest 缺失（vendored 依赖被删？）"
elif head -n 60 "$doctest" | grep -qi "copyright\|MIT\|license"; then
    ok "$doctest 保留上游版权/许可证声明"
else
    fail "$doctest 前 60 行里找不到版权或许可证声明"
fi

echo "── 3/3 自有源码无不相容许可证"
# 只在自有源码里查；docs/ 会出现"GPL"等字样（讨论兼容性时提到），故排除。
if hits=$(grep -rIl -E '\b(GPL|AGPL|LGPL)\b' \
    --include='*.hpp' --include='*.cpp' \
    include src examples tests bench 2>/dev/null); then
    fail "源码里出现 GPL 家族字样（与 Apache-2.0 不相容或需显式说明）："
    echo "$hits" | sed 's/^/      /' >&2
else
    ok "include/ src/ examples/ tests/ bench/ 无 GPL 家族字样"
fi

if [ "$status" -ne 0 ]; then
    echo "许可证检查失败。" >&2
else
    echo "许可证检查通过。"
fi
exit "$status"
