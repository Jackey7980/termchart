"""把 docs/gallery 的终端输出快照渲染成"终端截图"PNG（README 配图用）。

为什么需要它：README 里的图应该是"终端里的真身"，而不是另画一套。docs/gallery/*.txt
是 `tools/gen_gallery.py` 生成并校验过的**标准输出快照**——直接按终端的样子把它们画成
PNG，图与代码永远一致。

踩过的两个字体坑：
1. 盲文不能交给字体画：Consolas / DejaVu / Segoe UI Symbol 全都不带盲文字形（会画成
   豆腐块）。盲文本身是 Unicode 定义的 2×4 点阵，所以这里**按 Braille Patterns 的点位
   规则直接画点**——像素级准确，且与字体无关。
2. 中文要占 2 个字符格：等宽 CJK 字体（SimSun / 微软雅黑 / Noto CJK）一个汉字正好
   2 格，与终端一致。

用法：
    python tools/render_gallery_png.py        # 重新生成 docs/images/term_*.png

字体按候选顺序查找，找不到就跳过该类字符（绝不画成豆腐块）。不同机器字体不同 ⇒ 生成的
PNG 字节不完全一致但视觉一致；README 配图不必逐字节可复现。
"""
import io
import os

from PIL import Image, ImageDraw, ImageFont


def _first_font(candidates):
    for path in candidates:
        if os.path.exists(path):
            return path
    return None


# 等宽拉丁字体（ASCII / 框线 / 方块）。
FONT = _first_font([
    r"C:\Windows\Fonts\consola.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/dejavu/DejaVuSansMono.ttf",
    "/Library/Fonts/DejaVuSansMono.ttf",
])
# 等宽 CJK 字体：汉字占 2 格。
CJK_FONT = _first_font([
    r"C:\Windows\Fonts\simsun.ttc",
    r"C:\Windows\Fonts\msyh.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
])
BG, FG, TB, ACC = (13, 17, 23), (201, 209, 217), (22, 27, 34), (86, 156, 214)
JOBS = [
    ("06_dashboard", "term_dashboard", "termchart  |  dashboard (line + bar + heatmap)"),
    ("43_contour", "term_contour", "termchart  |  contour plot"),
    ("05_showcase", "term_showcase", "termchart  |  showcase (braille + halfblock)"),
    ("19_metro_map", "term_metro_map", "termchart  |  metro map"),
]


def render(name, out, title):
    with io.open("docs/gallery/%s.txt" % name, encoding="utf-8") as fp:
        lines = fp.read().rstrip("\n").split("\n")
    while lines and not lines[0].strip():
        lines.pop(0)
    while lines and not lines[-1].strip():
        lines.pop()
    if lines and lines[0].strip() and not lines[0].startswith((" ", "\t")) and len(lines) > 4:
        lines.pop(0)
    font = ImageFont.truetype(FONT, 15)
    bb = ImageDraw.Draw(Image.new("RGB", (8, 8))).textbbox((0, 0), "M", font=font)
    cw, lh = bb[2] - bb[0], int((bb[3] - bb[1]) * 1.42)
    cols = max(len(l) for l in lines)
    img = Image.new("RGB", (cols * cw + 36, 26 + len(lines) * lh + 36), BG)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, img.size[0], 26], fill=TB)
    for i, c in enumerate(((255, 95, 86), (255, 189, 46), (39, 201, 63))):
        d.ellipse([14 + i * 18, 9, 24 + i * 18, 19], fill=c)
    d.text((80, 5), title, font=font, fill=(139, 148, 158))
    y = 26 + 18
    # 盲文不交给字体画：本机没有带盲文字形的字体，而盲文本身就是 Unicode 定义的
    # 2x4 点阵——按 Braille Patterns 的点位规则直接画点，像素级准确且零字体依赖。
    # 点尺寸取满格距：相邻点相接，画出来才是"实笔画"（真实终端里盲文点也是连着的）。
    dw, dh = max(2, cw // 2), max(2, lh // 4)
    # 中文用等宽 CJK 字体：一个汉字正好占 2 个字符格（与终端一致）。
    cjk_font = ImageFont.truetype(CJK_FONT, int(round(cw * 2))) if CJK_FONT else None
    for line in lines:
        x = 18
        for ch in line:
            code = ord(ch)
            if 0x2800 <= code <= 0x28FF:
                bits = code - 0x2800
                ox = x + (cw - 2 * dw) // 2
                oy = y + (lh - 4 * dh) // 2
                for bit, (row, col) in enumerate(((0, 0), (0, 1), (0, 2), (1, 0), (1, 1), (1, 2), (3, 0), (3, 1))):
                    if bits & (1 << bit):
                        d.rectangle([ox + col * dw, oy + row * dh,
                                     ox + col * dw + dw - 1, oy + row * dh + dh - 1], fill=FG)
            elif code >= 0x3000:
                if cjk_font is not None:
                    d.text((x, y - 1), ch, font=cjk_font, fill=FG)
                x += cw * 2
                continue
            elif ch != " ":
                d.text((x, y), ch, font=font, fill=ACC if ch in "\u2500\u2502\u250c\u2510\u2514\u2518\u251c\u2524\u252c\u2534\u253c\u25c9\u25cf\u25cb\u25c6\u25b2\u25bc\u25c4\u25ba\u2572\u2571" else FG)
            x += cw
        y += lh
    path = "docs/images/%s.png" % out
    img.quantize(colors=256, method=Image.MEDIANCUT, dither=Image.NONE).save(path, format="PNG", optimize=True)
    print("%s %dx%d %.0fKB" % (path, img.size[0], img.size[1], os.path.getsize(path) / 1024))


for name, out, title in JOBS:
    render(name, out, title)
