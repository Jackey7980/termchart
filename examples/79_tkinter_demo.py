#!/usr/bin/env python3
"""examples/79_tkinter_demo.py -- 在 Tkinter 窗口里显示 termchart 的像素图

termchart 的 GUI 平行栈把图表渲染成 RGB(A) 像素缓冲；Python 侧拿这块缓冲，
交给 PIL 转成 Tkinter 能显示的 PhotoImage 即可嵌进任意 Tk 窗口。这条路径不依赖
任何 GUI 后端（Win32/Qt/Tk C 后端），只要有 Pillow：

    pip install pillow
    python examples/79_tkinter_demo.py

如果没有 Pillow，可改用 to_rgba() 拿到 numpy 数组后自行 blit，但 Pillow 是最省事的。
"""
import math
import tkinter as tk
from tkinter import ttk

import termchart

try:
    from PIL import Image, ImageTk
except ImportError:  # Pillow 缺失时给明确提示，而不是默默崩
    raise SystemExit("本示例需要 Pillow：pip install pillow")


def main() -> None:
    # 1) 让 termchart 把图表渲染成 RGB 像素字节（无需 numpy）。
    w, h = 640, 360
    xs = [i * 0.1 for i in range(64)]
    ys = [math.sin(x) for x in xs]
    # to_rgb_bytes 直接产出 (w*h*3) 的 RGB 字节，正好喂给 Image.frombytes。
    rgb = termchart.to_rgb_bytes(xs, ys, title="sin(x)", width=80, height=24,
                                pixel_width=w, pixel_height=h)
    image = Image.frombytes("RGB", (w, h), rgb)

    root = tk.Tk()
    root.title("termchart in Tkinter")

    canvas = tk.Canvas(root, width=w, height=h)
    canvas.pack(padx=8, pady=8)
    photo = ImageTk.PhotoImage(image)  # 必须在 Tk 实例创建后才能构造
    canvas.create_image(0, 0, anchor="nw", image=photo)

    ttk.Button(root, text="关闭", command=root.destroy).pack(pady=4)
    # 保留对 photo 的引用，否则被 GC 后画布显示空白。
    canvas.image = photo  # type: ignore[attr-defined]
    root.mainloop()


if __name__ == "__main__":
    main()
