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

def plot_df(
    df: Any,
    x: str = "",
    y: str = "",
    title: str = "",
    width: int = 80,
    height: int = 24,
) -> str:
    """画 DataFrame 的两列（df[x], df[y]）；x 留空则用索引。返回 ANSI 文本。"""

def plot_series(series: Any, title: str = "", width: int = 80, height: int = 24) -> str:
    """画 pandas Series（索引 -> x，值 -> y）。返回 ANSI 文本。"""

def to_rgba_bytes(
    x: Any,
    y: Any,
    title: str = "",
    width: int = 80,
    height: int = 24,
    pixel_width: int = 960,
    pixel_height: int = 540,
) -> bytes:
    """渲染并把 RGBA 像素作为字节返回（无需 numpy），供 PIL / Tkinter 等再合成。"""

def to_rgb_bytes(
    x: Any,
    y: Any,
    title: str = "",
    width: int = 80,
    height: int = 24,
    pixel_width: int = 960,
    pixel_height: int = 540,
) -> bytes:
    """渲染并把 RGB 像素作为字节返回（无需 numpy）。"""

def to_rgba(
    x: Any,
    y: Any,
    title: str = "",
    width: int = 80,
    height: int = 24,
    pixel_width: int = 960,
    pixel_height: int = 540,
) -> Any:
    """渲染并把 RGBA 像素作为 numpy (H, W, 4) uint8 数组返回（需 numpy）。"""

def to_rgb(
    x: Any,
    y: Any,
    title: str = "",
    width: int = 80,
    height: int = 24,
    pixel_width: int = 960,
    pixel_height: int = 540,
) -> Any:
    """渲染并把 RGB 像素作为 numpy (H, W, 3) uint8 数组返回（需 numpy）。"""

def to_png_bytes(
    x: Any,
    y: Any,
    title: str = "",
    width: int = 80,
    height: int = 24,
    pixel_width: int = 960,
    pixel_height: int = 540,
) -> bytes:
    """渲染并把 PNG 文件字节返回。"""

def save_png_pixels(pixels: Any, width: int, height: int, path: str) -> None:
    """把一个 w*h*4 的 RGBA 像素缓冲（bytes 或 uint8 ndarray）编码成 PNG 文件。"""

def show_qt(
    x: Any,
    y: Any,
    title: str = "",
    width: int = 80,
    height: int = 24,
    pixel_width: int = 960,
    pixel_height: int = 540,
) -> None:
    """开一个 Qt 窗口显示图表（仅限启用了 Qt 的构建；默认 wheel 不含）。"""
