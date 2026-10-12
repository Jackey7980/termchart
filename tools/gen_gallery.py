#!/usr/bin/env python3
"""gen_gallery.py —— 生成 docs/gallery/ 示例输出快照与 docs/gallery.md 索引。

产物是生成物：扫描 examples/CMakeLists.txt 的示例清单，对每个示例在
--build-dir（默认 build/debug-windows）里运行，捕获 stdout、剥掉 ANSI 后
写入 docs/gallery/<示例名>.txt，并汇总成 docs/gallery.md 索引
（示例名 → 一句话职责 ← 取自示例源文件首行 `// <名>.cpp —— 职责`）。

示例输出依赖终端尺寸的（如 03_function_plot）在此拿到的是非 tty 下的默认
画布输出，作为"外观指纹"仍然稳定：示例本体不按 tty 探测改变构图，只按
stdout 是否为 tty 调整颜色降级，剥 ANSI 后不影响文本内容。

--check 模式不写文件，只比对现有产物与重新生成的结果，不一致即非零退出
（与 tools/_genutil.py 的仓库夹具约定一致）。
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

from _genutil import ROOT, emit, wants_check

EXAMPLES_CMAKE = ROOT / "examples" / "CMakeLists.txt"
GALLERY_DIR = ROOT / "docs" / "gallery"

# CSI 序列 + OSC 序列 + 其余单字符转义（含 \x1b(B 等字符集切换）。
ANSI_RE = re.compile(
    r"\x1b\[[0-9;:?]*[ -/]*[@-~]"          # CSI
    r"|\x1b\][^\x07\x1b]*(?:\x07|\x1b\\)?"  # OSC（BEL 或 ST 结束）
    r"|\x1b[@-Z\\-_]"                        # 其余转义（含字符集切换）
)


def strip_ansi(text: str) -> str:
    # \r 要归一化：渲染器用 \r 做"行内覆盖"（diff 帧重绘状态行），
    # 且行尾常出现 \r\r\n。剥掉 ANSI 后若保留 \r，emit 的 read_text/write_text
    # 会做换行翻译（\r 也算行结束符），导致写入与回读不一致、--check 永远红。
    text = ANSI_RE.sub("", text)
    return text.replace("\r\n", "\n").replace("\r", "")


def example_list() -> list[str]:
    """从 examples/CMakeLists.txt 的第一个 foreach(example ...) 提取清单。"""
    text = EXAMPLES_CMAKE.read_text(encoding="utf-8")
    m = re.search(r"foreach\(example((?:\s+\w+)+)\)", text)
    if not m:
        raise SystemExit(f"无法从 {EXAMPLES_CMAKE.relative_to(ROOT)} 解析示例清单")
    return m.group(1).split()


def description_of(name: str) -> str:
    """职责一句话：源文件首行 `// <名>.cpp —— 职责`，找不到则明确标注。"""
    src = ROOT / "examples" / f"{name}.cpp"
    if src.exists():
        head = src.read_text(encoding="utf-8", errors="replace").splitlines()[:5]
        for line in head:
            m = re.match(r"^//\s*" + re.escape(name) + r"\.cpp\s*——\s*(.+?)\s*$", line)
            if m:
                return m.group(1)
    return "（源文件首行注释未写职责，待补）"


def find_binary(build_dir: Path, name: str) -> Path:
    base = build_dir / "examples" / name
    for cand in (base, base.with_suffix(".exe")):
        if cand.is_file():
            return cand
    raise SystemExit(
        f"[缺少二进制] {base.with_suffix('.exe').relative_to(ROOT)} —— "
        f"请先构建（cmake --build --preset <预设>），或用 --build-dir 指定别的构建目录"
    )


def run_example(binary: Path) -> str:
    proc = subprocess.run(
        [str(binary)],
        capture_output=True,
        timeout=120,
        cwd=str(ROOT),
    )
    if proc.returncode != 0:
        raise SystemExit(
            f"[运行失败] {binary.name} 退出码 {proc.returncode}\n"
            + proc.stderr.decode("utf-8", errors="replace")[:2000]
        )
    return strip_ansi(proc.stdout.decode("utf-8", errors="replace"))


def gallery_index(entries: list[tuple[str, str]]) -> str:
    lines = [
        "<!-- 由 tools/gen_gallery.py 生成，禁手改。更新方式：重跑该脚本。 -->",
        "# 示例画廊",
        "",
        "每个示例在非 tty 环境下运行的 stdout 快照（已剥 ANSI，文本内容即构图）。",
        "二进制来自 `--build-dir` 指定的构建目录；职责一句话取自示例源文件首行注释。",
        "",
        "| 示例 | 职责 | 快照 |",
        "| --- | --- | --- |",
    ]
    for name, desc in entries:
        lines.append(f"| `{name}` | {desc} | [文本快照](gallery/{name}.txt) |")
    lines.append("")
    return "\n".join(lines) + "\n"


def main(argv: list[str]) -> int:
    check = wants_check(argv)
    build_dir = ROOT / "build" / "debug-windows"
    if "--build-dir" in argv:
        i = argv.index("--build-dir")
        build_dir = Path(argv[i + 1])
        if not build_dir.is_absolute():
            build_dir = ROOT / build_dir

    changed = False
    entries: list[tuple[str, str]] = []
    for name in example_list():
        out = run_example(find_binary(build_dir, name))
        changed |= emit(GALLERY_DIR / f"{name}.txt", out, check)
        entries.append((name, description_of(name)))

    changed |= emit(ROOT / "docs" / "gallery.md", gallery_index(entries), check)
    return 1 if changed and check else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
