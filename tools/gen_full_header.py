#!/usr/bin/env python3
# 生成 include/termchart/termchart_full.hpp —— 全量伞形头。
# 直接 glob 所有公共头，避免手工漏列/错列，新增头也会自动纳入。
import glob
import os

ROOT = r"c:/Users/琳琳/Desktop/刘梓祺/作品/termchart/include/termchart"
SKIP = {"termchart.hpp", "termchart_full.hpp"}

headers = sorted(glob.glob(os.path.join(ROOT, "**", "*.hpp"), recursive=True))
rel = []
for h in headers:
    if os.path.basename(h) in SKIP:
        continue
    rel.append(os.path.relpath(h, ROOT).replace("\\", "/"))

lines = [
    "// termchart_full.hpp —— 全量伞形头（自动生成，请勿手改；见 tools/gen_full_header.py）",
    "// 包含整个 termchart 公共 API：termchart.hpp 覆盖 core/util/color/term/render/api，",
    "// 这里再补齐 chart/graph/scale/raster/dsl/widget/gui/backend/interaction 等全部子模块。",
    "#ifndef TERMCHART_FULL_HPP",
    "#define TERMCHART_FULL_HPP",
    "",
    '#include "termchart/termchart.hpp"',
]
for r in rel:
    lines.append(f'#include "termchart/{r}"')
lines += ["", "namespace tc = termchart;", "", "#endif  // TERMCHART_FULL_HPP"]

with open(os.path.join(ROOT, "termchart_full.hpp"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
print(f"generated termchart_full.hpp with {len(rel)} includes")
