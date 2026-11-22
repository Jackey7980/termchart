"""termchart —— 零依赖终端图表库（matplotlib 式一行出图）。

零配置起步::

    import termchart as tc
    print(tc.plot([0, 1, 2, 3], [1, 4, 2, 8]))

要文件::

    tc.save_png([0, 1, 2, 3], [1, 4, 2, 8], "out.png")
    tc.save_svg([0, 1, 2, 3], [1, 4, 2, 8], "out.svg")
"""

from typing import Any, List, Optional, Sequence

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
    values: Sequence[float],
    labels: Optional[Sequence[str]] = None,
    title: str = "",
    width: int = 80,
    height: int = 24,
) -> str:
    """画柱状图，返回 ANSI 文本。labels 可省略（按序号补）。"""

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
    values: Sequence[float],
    labels: Optional[Sequence[str]] = None,
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

# ── 通用图表模型（全部图种）──────────────────────────────────────────────────

class Chart:
    """按 kind 创建的通用图表，覆盖 tc_chart_kinds() 的全部图种。

    五种数据形状与 C ABI 一一对应（add_points / add_series / set_values /
    set_matrix / set_scalar / set_tree / set_text / set_regions），渲染走同一条
    C++ 实现，因此 ANSI / SVG / PNG 与 C++ 侧逐字节相同。
    """

    def __init__(self, kind: str) -> None: ...
    @property
    def kind(self) -> str: ...
    def add_points(self, x: Any, y: Any) -> None: ...
    def add_series(
        self,
        x: Any,
        y: Any,
        name: str = "",
        color: Optional[Sequence[int]] = None,
    ) -> None:
        """追加一条系列；color 为 (r, g, b)，省略则用调色板默认色。"""
    def add_many(self, series: Any) -> None:
        """批量追加：(x, y[, name[, (r,g,b)]]) 的序列。"""
    def set_values(self, values: Any, labels: Optional[Sequence[str]] = None) -> None: ...
    def set_matrix(
        self,
        rows: Any,
        row_labels: Optional[Sequence[str]] = None,
        col_labels: Optional[Sequence[str]] = None,
    ) -> None: ...
    def set_scalar(self, value: float) -> None: ...
    def set_tree(self, nodes: Any) -> None:
        """(parent_index, name, value) 的序列（-1 为根）。"""
    def set_text(self, rows: Any, blob: Optional[str] = None) -> None:
        """文本矩阵：rows 为二维字符串序列，blob 供 ASCII 图使用。"""
    def set_regions(self, regions: Any) -> None:
        """(name, value[, cells]) 的序列。"""
    def set_prop(self, prop: str, value: Any) -> None:
        """属性通道：y_domain / auto_domain / include_zero / emphasize_zero /
        shade_negative / x_ticks / x_label / y_label。"""
    def ansi(
        self,
        width: Optional[int] = None,
        height: Optional[int] = None,
        title: Optional[str] = None,
        show_legend: Optional[bool] = None,
        depth: str = "truecolor",
    ) -> str: ...
    def print(
        self,
        width: Optional[int] = None,
        height: Optional[int] = None,
        title: Optional[str] = None,
        show_legend: Optional[bool] = None,
        depth: str = "truecolor",
    ) -> None: ...
    def to_svg(
        self,
        width: Optional[int] = None,
        height: Optional[int] = None,
        title: Optional[str] = None,
        show_legend: Optional[bool] = None,
    ) -> str: ...
    def to_png(
        self,
        width: Optional[int] = None,
        height: Optional[int] = None,
        title: Optional[str] = None,
        show_legend: Optional[bool] = None,
    ) -> bytes: ...
    def pixels(
        self,
        width: Optional[int] = None,
        height: Optional[int] = None,
        title: Optional[str] = None,
        show_legend: Optional[bool] = None,
    ) -> Any:
        """渲染结果为 numpy (H, W, 4) uint8 数组。"""

def kinds() -> List[str]:
    """全部已登记的图种名（字典序）。新增图种会自动出现在这里。"""

def auto_plot(path: str) -> None:
    """读数据文件（CSV / JSON）自动选图种并打印。"""

# ── 图像 ─────────────────────────────────────────────────────────────────

class _Bitmap:
    """对应 image::Bitmap：3/4 通道、行紧致。像素操作在 C 侧完成。"""

    @staticmethod
    def create(width: int, height: int, channels: int = 4) -> "_Bitmap": ...
    @staticmethod
    def load(path: str) -> "_Bitmap": ...
    @staticmethod
    def decode(data: bytes) -> "_Bitmap": ...
    @property
    def width(self) -> int: ...
    @property
    def height(self) -> int: ...
    @property
    def channels(self) -> int: ...
    def pixels(self) -> bytes: ...
    def numpy(self) -> Any: ...
    def scaled_to(self, width: int, height: int) -> "_Bitmap": ...
    def with_alpha(self, alpha: int = 255) -> "_Bitmap": ...
    def without_alpha(self) -> "_Bitmap": ...
    def fill(self, r: int, g: int, b: int, a: int = 255) -> None: ...
    def fill_rect(
        self, x: int, y: int, w: int, h: int, r: int, g: int, b: int, a: int = 255
    ) -> None: ...
    def blit_over(self, src: "_Bitmap", dx: int, dy: int) -> None: ...
    def encode(self, format: str = "png", quality: int = 85) -> bytes: ...
    def save(self, path: str, format: str = "", quality: int = 85) -> None: ...

class _ImageModule:
    Bitmap: Any
    def load(self, path: str) -> _Bitmap: ...
    def decode(self, data: bytes) -> _Bitmap: ...

image: _ImageModule
