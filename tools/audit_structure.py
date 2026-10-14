#!/usr/bin/env python3
# 审计 v2：结构.md 规划 vs 磁盘实际（目录级文件数对比 + 库文件缺失 + 极短文件）
import os, re

ROOT = r"c:/Users/琳琳/Desktop/刘梓祺/作品/termchart"
STRUCT = os.path.join(ROOT, "结构.md")
INC = os.path.join(ROOT, "include", "termchart")

text = open(STRUCT, encoding="utf-8").read()

# ---- A. 目录级：结构.md 用 "###### dir/(N)" 标记的子目录 ----
planned_dirs = {}
for m in re.finditer(r'######\s+([A-Za-z0-9_]+)/\s*[（(]\s*(\d+)\s*[）)]', text):
    planned_dirs[m.group(1)] = int(m.group(2))

# 排除明显的非 include 目录名（如 examples 的数字章节不会出现 ###### dir/(N)）
rows = []
for name, N in sorted(planned_dirs.items()):
    actual_dir = None
    for b, dirs, files in os.walk(INC):
        if os.path.basename(b) == name:
            actual_dir = b
            break
    cnt = 0
    if actual_dir:
        cnt = len([f for f in os.listdir(actual_dir)
                   if f.endswith(".hpp") and os.path.isfile(os.path.join(actual_dir, f))])
    diff = cnt - N
    flag = "OK" if diff == 0 else (f"缺{abs(diff)}" if diff < 0 else f"多{diff}")
    rows.append((name, N, cnt, flag, actual_dir))

# ---- B. 库文件缺失（basename 比对，排除非库区域）----
EXCLUDE_PREFIX = ("examples", "tests", "bench", "fuzz", "tools", "single_include",
                  "third_party", "assets", "docs")
EXCLUDE_NAME = lambda f: (
    f.startswith(("test_", "bench_", "fuzz_", "replace_"))
    or f[0:2].isdigit()                       # 示例 01_..54_
    or f.endswith("_demo.cpp")
    or f in ("main.cpp", "golden_runner.hpp", "sample_data.hpp",
             "sample_graph.hpp", "termchart.cpp", "termchart_full.cpp")
)
actual_hpp = {}
for b, dirs, files in os.walk(INC):
    if any(p in EXCLUDE_PREFIX for p in b.split(os.sep)):
        continue
    for f in files:
        if f.endswith(".hpp"):
            actual_hpp.setdefault(f, os.path.relpath(os.path.join(b, f), INC).replace("\\", "/"))

planned_hpp = set(re.findall(r"[A-Za-z0-9_/]+\.hpp", text))
planned_hpp_names = {os.path.basename(p) for p in planned_hpp}
# 只保留看起来像库头的（去掉 docs/reference 等提到的，但基本都是）
missing_hpp = sorted(planned_hpp_names - set(actual_hpp))
# 去掉 fixtures/golden 之类仍混入的
missing_hpp = [f for f in missing_hpp if not EXCLUDE_NAME(f)]

# GUI 平行栈
gui_exists = os.path.isdir(os.path.join(ROOT, "include", "termchart_gui"))

# ---- C. 极短文件（<15 行，真嫌疑）----
tiny = []
for f, rel in sorted(actual_hpp.items()):
    p = os.path.join(INC, rel)
    try:
        n = sum(1 for _ in open(p, encoding="utf-8", errors="ignore"))
    except Exception:
        continue
    if n < 15:
        tiny.append((rel, n))

# ---- 输出 ----
L = []
L.append("########## A. 目录级文件数对比（结构.md ###### dir/(N) 标记）##########")
for name, N, cnt, flag, ad in rows:
    L.append(f"  {name:14} 规划{N:3} 实际{cnt:3}  {flag}   {ad or '(目录未找到)'}")
L.append("")
L.append("########## B. 库头文件缺失（结构.md 规划但 include/termchart 无同名，已排除 examples/tests/bench/fuzz 等）##########")
L.append("\n".join(missing_hpp) or "(无)")
L.append("")
L.append("########## C. GUI 平行栈（include/termchart_gui）##########")
L.append(f"  目录存在: {gui_exists}  -> {'未开工（25 文件全缺）' if not gui_exists else '已开工'}")
L.append("")
L.append("########## D. 极短库头（<15 行，可能是虚表锚点/聚合，需人工确认非真空壳）##########")
for rel, n in tiny:
    L.append(f"  {rel}  行数={n}")
if not tiny:
    L.append("(无)")

out = "\n".join(L)
open(os.path.join(ROOT, "audit_report.txt"), "w", encoding="utf-8").write(out)
print(out)
