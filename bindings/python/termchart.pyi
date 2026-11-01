"""termchart —— 零依赖终端图表库（matplotlib 式一行出图）。

零配置起步::

    import termchart as tc
    print(tc.plot([0, 1, 2, 3], [1, 4, 2, 8]))

要文件::

    tc.save_png([0, 1, 2, 3], [1, 4, 2, 8], "out.png")
    tc.save_svg([0, 1, 2, 3], [1, 4, 2, 8], "out.svg")
"""

from typing import Sequence

__version__: str

def version() -> str:
    """返回 termchart 版本号字符串。"""

def plot(
    x: Sequence[float],
    y: Sequence[float],
    title: str = "",
    width: int = 80,
    height: int = 24,
) -> str:
    """画折线图，返回 ANSI 文本（终端字符格）。"""

def scatter(
    x: Sequence[float],
    y: Sequence[float],
    title: str = "",
    width: int = 80,
    height: int = 24,
) -> str:
    """画散点图，返回 ANSI 文本。"""

def bar(
    labels: Sequence[str],
    values: Sequence[float],
    title: str = "",
    width: int = 80,
    height: int = 24,
) -> str:
    """画柱状图，返回 ANSI 文本。labels 可为空（按序号补）。"""

def plot_svg(
    x: Sequence[float],
    y: Sequence[float],
    title: str = "",
    width: int = 80,
    height: int = 24,
) -> str:
    """画折线图，返回 SVG 文本。"""

def scatter_svg(
    x: Sequence[float],
    y: Sequence[float],
    title: str = "",
    width: int = 80,
    height: int = 24,
) -> str:
    """画散点图，返回 SVG 文本。"""

def bar_svg(
    labels: Sequence[str],
    values: Sequence[float],
    title: str = "",
    width: int = 80,
    height: int = 24,
) -> str:
    """画柱状图，返回 SVG 文本。"""

def save_svg(
    x: Sequence[float],
    y: Sequence[float],
    path: str,
    title: str = "",
    width: int = 80,
    height: int = 24,
) -> None:
    """画折线图并写成 SVG 文件。"""

def save_png(
    x: Sequence[float],
    y: Sequence[float],
    path: str,
    title: str = "",
    width: int = 80,
    height: int = 24,
    pixel_width: int = 960,
    pixel_height: int = 540,
) -> None:
    """画折线图并写成 PNG 文件（chart -> Scene -> 位图 -> PNG）。"""

def render_line(text: str, width: int = 0) -> str:
    """把一段文本渲染成带样式的 ANSI 终端行。"""
