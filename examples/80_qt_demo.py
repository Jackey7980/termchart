#!/usr/bin/env python3
"""examples/80_qt_demo.py -- 用 termchart 开一个 Qt 窗口出图（需 Qt 版构建）

termchart.show_qt 仅在**启用了 Qt 的构建**里存在（cmake 开启
TERMCHART_BUILD_GUI_QT 且系统有 Qt，Python 模块会自动链入并暴露 show_qt）。
默认 wheel 不含 Qt（Qt 是 LGPL 动态链接，需使用者自备），此时本示例运行会抛
AttributeError("module 'termchart' has no attribute 'show_qt'")。

    # 构建时：TERMCHART_BUILD_GUI_QT=ON 编译出含 show_qt 的模块
    python examples/80_qt_demo.py
"""
import math

import termchart


def main() -> None:
    if not hasattr(termchart, "show_qt"):
        raise SystemExit(
            "当前 termchart 构建未包含 Qt 支持（TERMCHART_BUILD_GUI_QT=OFF）。"
        )
    xs = [i * 0.1 for i in range(64)]
    ys = [math.cos(x) for x in xs]
    # 开一个独立 Qt 窗口显示图表，阻塞到窗口关闭。
    termchart.show_qt(xs, ys, title="cos(x)", width=80, height=24,
                     pixel_width=960, pixel_height=540)


if __name__ == "__main__":
    main()
