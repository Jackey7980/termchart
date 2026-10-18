#!/usr/bin/env python3
"""examples/71_python_demo.py —— Python 端一行出图 demo

仅当 `pip install -e bindings/python` 后可用：

    pip install -e bindings/python
    python examples/71_python_demo.py

不需要重定向即可看到输出（termchart 默认打印到 stdout）。
"""
import math
import termchart


def main() -> None:
    # 折线图：sin(x)
    xs = [i * 0.1 for i in range(64)]
    ys = [math.sin(x) for x in xs]
    print(termchart.plot(xs, ys, title="sin(x)"))

    # 柱状图：前 10 个字母频次（虚构数据）
    letters = ["a", "b", "c", "d", "e", "f", "g", "h", "i", "j"]
    counts  = [12, 18, 5,  9,  20, 7,  3, 14, 22, 6]
    print(termchart.bar(letters, counts, title="letter freq"))

    # 散点图：cos / sin 圆形
    cx = [math.cos(i * 0.2) for i in range(30)]
    cy = [math.sin(i * 0.2) for i in range(30)]
    print(termchart.scatter(cx, cy, title="unit circle"))


if __name__ == "__main__":
    main()
