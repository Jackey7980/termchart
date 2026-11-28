"""termchart_capi —— 基于 C ABI 的 Python 绑定（ctypes）。

对应 C 侧 termchart_capi 共享库（见 libs/capi）。_capi 模块由
tools/gen_python_capi.py 从 termchart_capi.h 生成（加载库 + ctypes 原型）；
本模块再裹一层 Pythonic API，能力面与 C++ 便捷入口 api:: 对齐：

    文本    render_line / print_line / hello（可带颜色与属性）
    图表    plot / scatter / bar（返回 ANSI 字符串） + print_chart（直接写 stdout）
    句柄    Chart（多系列 / 值 / 矩阵 + 属性通道 → ANSI·SVG·PNG·终端协议）
    图像    Image / load_image / decode_image（PNG·JPEG·GIF·WebP + 像素操作）
    协议    detect_protocol / Chart.to_protocol / Image.to_protocol（sixel·kitty·iterm2·半块）
    关系图  Graph / graph_from_text（mermaid·dot·d2·plantuml → 12 种图种）
    文档    Document / document（HTML·Markdown·纯文本，可嵌图表块）
    输出    write_stdout / write_stderr / flush
    自动    auto_plot（读 CSV/JSONL → AutoViz → 出图）
    其他    version

共享库定位顺序：TERMCHART_CAPI_LIB 环境变量 → 仓库 build/ 目录递归搜索
libtermchart_capi.{dll,so,dylib} → 系统 PATH。
"""
from __future__ import annotations

import ctypes

from . import _capi as _c

__all__ = [
    "plot", "scatter", "bar", "print_chart", "Chart", "kinds",
    "render_line", "print_line", "hello",
    "write_stdout", "write_stderr", "flush",
    "auto_plot", "version", "TermchartError",
    "Image", "load_image", "decode_image",
    "detect_protocol", "Graph", "graph_from_text", "GRAPH_KINDS",
    "Document", "document",
    "term_size", "term_size_from_env", "term_caps", "stdout_is_tty", "stdin_is_tty",
    "clipboard_set", "hyperlink", "hyperlink_close", "set_mouse", "Terminal",
    "Canvas", "Renderer", "InputParser", "Regions",
    "chart_export", "chart_to_image", "SequenceDiagram", "sequence_from_text",
    "encode_frames", "Scene3D",
]

_c._load()  # 加载共享库（自动搜索常见构建目录 / TERMCHART_CAPI_LIB）

_DEPTH = {
    "none": _c.TC_DEPTH_NONE,
    "16": _c.TC_DEPTH_16,
    "256": _c.TC_DEPTH_256,
    "truecolor": _c.TC_DEPTH_TRUECOLOR,
}

_ATTR = {"bold": 1, "dim": 2, "italic": 4, "underline": 8, "reverse": 16}

# 终端协议：与 C++ protocol::ProtocolKind 一一对应（AUTO 走同一条 detect/pick 降级链）。
_PROTO = {
    "auto": _c.TC_PROTO_AUTO,
    "sixel": _c.TC_PROTO_SIXEL,
    "kitty": _c.TC_PROTO_KITTY,
    "iterm2": _c.TC_PROTO_ITERM2,
    "halfblock": _c.TC_PROTO_HALFBLOCK,
}

# 关系图属性通道：字符串名 → tc_graph_prop（新增属性 = 这里加一行，Python 面不动）。
_GRAPH_PROP = {
    "shape": _c.TC_GRAPH_PROP_SHAPE,
    "route": _c.TC_GRAPH_PROP_ROUTE,
    "edge_labels": _c.TC_GRAPH_PROP_EDGE_LABELS,
    "arrows": _c.TC_GRAPH_PROP_ARROWS,
    "node_border": _c.TC_GRAPH_PROP_NODE_BORDER,
    "min_width": _c.TC_GRAPH_PROP_MIN_WIDTH,
    "min_height": _c.TC_GRAPH_PROP_MIN_HEIGHT,
    "h_gap": _c.TC_GRAPH_PROP_H_GAP,
    "v_gap": _c.TC_GRAPH_PROP_V_GAP,
    "mark_cycles": _c.TC_GRAPH_PROP_MARK_CYCLES,
}

# shape / route 允许用名字，值对齐 graph::ShapeKind / RouteStyle。
_SHAPE = {"box": 0, "rounded": 1, "diamond": 2, "ellipse": 3,
          "stadium": 4, "hexagon": 5, "cylinder": 6, "icon": 7}
_ROUTE = {"straight": 0, "orthogonal": 1}


def _proto_kind(kind) -> int:
    key = (kind or "auto").lower()
    if key not in _PROTO:
        raise TermchartError(
            "unknown protocol %r (auto/sixel/kitty/iterm2/halfblock)" % (kind,)
        )
    return _PROTO[key]


def _protocol_opts(cell_width=10, cell_height=20, rle=True):
    po = _c.ProtocolOpts()
    po.cell_width = int(cell_width)
    po.cell_height = int(cell_height)
    po.rle = 1 if rle else 0
    return po


def _two_phase_str(call, what) -> str:
    """字符串两段式：call(buf_or_None, cap, byref(got), byref(err)) → 文本。"""
    need = ctypes.c_uint64(0)
    err = ctypes.c_void_p()
    st = call(None, 0, ctypes.byref(need), ctypes.byref(err))
    if st != 0:
        _raise(what, st, err)
    buf = ctypes.create_string_buffer(need.value + 1)
    got = ctypes.c_uint64(0)
    st = call(ctypes.cast(buf, ctypes.c_char_p), need.value, ctypes.byref(got), ctypes.byref(err))
    if st != 0:
        _raise(what, st, err)
    return bytes(buf.raw[: got.value]).decode("utf-8", "replace")


def _two_phase_bytes(call, what) -> bytes:
    """字节两段式（PNG / 协议流）。"""
    need = ctypes.c_uint64(0)
    err = ctypes.c_void_p()
    st = call(None, 0, ctypes.byref(need), ctypes.byref(err))
    if st != 0:
        _raise(what, st, err)
    buf = ctypes.create_string_buffer(need.value + 1)
    got = ctypes.c_uint64(0)
    st = call(ctypes.cast(buf, ctypes.c_char_p), need.value, ctypes.byref(got), ctypes.byref(err))
    if st != 0:
        _raise(what, st, err)
    return bytes(buf.raw[: got.value])


# ── 终端协议（对齐 C++ protocol::detect_protocol_caps / pick_protocol）──────────
#
# AUTO 是唯一会看环境的分支，且只在 C 侧看一次：Python 不必也不该自己猜终端。

def detect_protocol() -> str:
    """本机末端推荐协议名：``kitty`` / ``iterm2`` / ``sixel`` / ``halfblock`` / ``none``。"""
    return _two_phase_str(
        lambda buf, cap, need, err: _c.tc_protocol_detect(buf, cap, need, err),
        "detect_protocol",
    ).strip()


def _plot_opts(width=0, height=0, title=None, show_legend=False, depth="truecolor"):
    """tc_plot_opts：展示属性按次传，与模型属性（句柄内）互不重复。"""
    o = _c.PlotOpts()
    o.width = int(width or 0)
    o.height = int(height or 0)
    o.title = title.encode("utf-8") if title else None
    o.depth = _DEPTH.get(depth or "truecolor", _c.TC_DEPTH_TRUECOLOR)
    o.show_legend = 1 if show_legend else 0
    return o


class TermchartError(RuntimeError):
    """C 侧渲染 / 数据错误，携带 tc_error_handle 的人类可读消息。"""


def version() -> str:
    s = _c.tc_version_string()
    return s.decode("utf-8") if isinstance(s, bytes) else s


# ── 错误处理辅助 ─────────────────────────────────────────────────────────────

def _raise(_what, rc, err) -> None:
    msg = ""
    if err:
        buf = ctypes.create_string_buffer(512)
        _c.tc_error_message(err, buf, 512)
        msg = buf.value.decode("utf-8", "replace") if buf.value else ""
        _c.tc_error_free(err)
    raise TermchartError("%s failed (%d): %s" % (_what, rc, msg))


def _fill_color(dst, spec) -> None:
    """spec: None | (r,g,b) | int(0..15 → ansi16, 其余 → indexed)。"""
    if spec is None:
        return
    if isinstance(spec, (tuple, list)) and len(spec) == 3:
        dst.kind = _c.TC_COLOR_RGB
        dst.r, dst.g, dst.b = int(spec[0]), int(spec[1]), int(spec[2])
    elif isinstance(spec, int):
        if 0 <= spec <= 15:
            dst.kind = _c.TC_COLOR_ANSI16
            dst.index = spec
        else:
            dst.kind = _c.TC_COLOR_INDEXED
            dst.index = spec
    else:
        raise TermchartError("color spec 只接受 None / (r,g,b) / int")


def _make_style(fg=None, bg=None, attrs=0):
    st = _c.tc_style_default()
    _fill_color(st.fg, fg)
    _fill_color(st.bg, bg)
    if isinstance(attrs, int):
        st.attrs |= attrs
    elif attrs:
        if isinstance(attrs, str):
            attrs = [attrs]
        for a in attrs:
            st.attrs |= _ATTR.get(a, a if isinstance(a, int) else 0)
    return st


def _make_api_opts(width=0, height=0, depth="truecolor", unicode=True,
                   newline=True, reset_at_end=True):
    o = _c.tc_api_opts_default()
    o.width = int(width)
    o.height = int(height)
    o.depth = _DEPTH.get(depth, _c.TC_DEPTH_TRUECOLOR)
    o.has_depth = 1
    o.unicode = 1 if unicode else 0
    o.newline = 1 if newline else 0
    o.reset_at_end = 1 if reset_at_end else 0
    return o


# ── 图表（折线 / 散点 / 柱状）────────────────────────────────────────────────

def _quick_opts(width, height, depth, title, show_legend):
    opts = _c.PlotOpts()
    opts.width = int(width or 0)
    opts.height = int(height or 0)
    opts.title = title.encode("utf-8") if title else None
    opts.depth = _DEPTH.get(depth or "truecolor", _c.TC_DEPTH_TRUECOLOR)
    opts.show_legend = 1 if show_legend else 0
    return opts


def _quick_two_phase(call) -> str:
    """tc_quick_* 的两段式缓冲：先问长度，再取字节。"""
    need = ctypes.c_uint64(0)
    st = call(None, 0, ctypes.byref(need), None)
    if st != _c.TC_ERR_BUFFER_TOO_SMALL:
        raise TermchartError("quick render failed (status %d)" % (st,))
    buf = ctypes.create_string_buffer(need.value)
    got = ctypes.c_uint64(0)
    err = ctypes.c_void_p()
    st = call(buf, need.value, ctypes.byref(got), ctypes.byref(err))
    if st != 0:
        _raise("quick render", st, err)
    return bytes(buf.raw[: got.value]).decode("utf-8")


def _render(kind: str, **data) -> str:
    """一行式出图：直通 tc_quick_*，也就是 C++ 的 api::render_plot / render_scatter / render_bar。

    绑定层**不**自己拼 Chart——拼出来系列名、配色、图例默认值会和 C++ 那一版
    对不上（实测就差了这两处）。走同一函数，输出才逐字节相同。
    """
    opts = _quick_opts(
        data.get("width"), data.get("height"), data.get("depth"),
        data.get("title"), data.get("show_legend"),
    )
    if kind == "bar":
        values = list(data["values"])
        n = len(values)
        vals = (ctypes.c_double * n)(*values)
        labels = data.get("labels")
        arr = (ctypes.c_char_p * len(labels))(*[l.encode("utf-8") for l in labels]) if labels else None
        text = _quick_two_phase(
            lambda buf, cap, need, err: _c.tc_quick_bar(
                vals, arr, n, ctypes.byref(opts), buf, cap, need, err
            )
        )
    else:
        x = list(data["x"])
        y = list(data["y"])
        n = min(len(x), len(y))
        xs = (ctypes.c_double * n)(*x[:n])
        ys = (ctypes.c_double * n)(*y[:n])
        quick_kind = _c.TC_QUICK_SCATTER if kind == "scatter" else _c.TC_QUICK_LINE
        text = _quick_two_phase(
            lambda buf, cap, need, err: _c.tc_quick_xy(
                quick_kind, xs, ys, n, ctypes.byref(opts), buf, cap, need, err
            )
        )

    if data.get("to_stdout"):
        sys.stdout.write(text)
        return ""
    return text


def plot(x, y, *, title=None, width=0, height=0, depth="truecolor", show_legend=False) -> str:
    """折线图：(x[i], y[i]) 连线 + 数据点标记。返回 ANSI 字符串（UTF-8 + 真彩 SGR）。"""
    return _render(
        "line", x=x, y=y, title=title, width=width, height=height,
        depth=depth, show_legend=show_legend,
    )


def scatter(x, y, *, title=None, width=0, height=0, depth="truecolor", show_legend=False) -> str:
    """散点图：仅标记每个点，不连线。返回 ANSI 字符串。"""
    return _render(
        "scatter", x=x, y=y, title=title, width=width, height=height,
        depth=depth, show_legend=show_legend,
    )


def bar(values, labels=None, *, title=None, width=0, height=0, depth="truecolor",
        show_legend=False) -> str:
    """柱状图：values[i]（labels 可选，缺省按序号）。返回 ANSI 字符串。"""
    return _render(
        "bar", values=values, labels=labels, title=title, width=width, height=height,
        depth=depth, show_legend=show_legend,
    )


def print_chart(x, y, *, kind="line", **kw) -> None:
    """对齐 api::plot/scatter：渲染并直接写 stdout（不返回字符串）。"""
    _render(kind, x=x, y=y, to_stdout=True, **kw)


# ── 文本（对齐 api::render_line / print_line / hello）────────────────────────

def render_line(text, *, fg=None, bg=None, attrs=0, width=0, height=0,
                depth="truecolor", unicode=True, newline=True, reset_at_end=True) -> str:
    """把一段文本按样式渲染成 ANSI 字节串（不写 stdout）。"""
    st = _make_style(fg, bg, attrs)
    o = _make_api_opts(width, height, depth, unicode, newline, reset_at_end)
    out = ctypes.c_char_p()
    err = ctypes.c_void_p()
    rc = _c.tc_render_line(
        text.encode("utf-8"), ctypes.byref(st), ctypes.byref(o),
        ctypes.byref(out), ctypes.byref(err),
    )
    if rc != 0:
        _raise("render_line", rc, err)
    result = out.value
    _c.tc_free_string(out)
    return result.decode("utf-8") if isinstance(result, bytes) else result


def print_line(text, *, fg=None, bg=None, attrs=0, width=0, height=0,
               depth="truecolor", unicode=True, newline=True, reset_at_end=True) -> None:
    """渲染并直接写 stdout（对齐 api::print_line）。"""
    st = _make_style(fg, bg, attrs)
    o = _make_api_opts(width, height, depth, unicode, newline, reset_at_end)
    err = ctypes.c_void_p()
    rc = _c.tc_print_line(text.encode("utf-8"), ctypes.byref(st), ctypes.byref(o),
                          ctypes.byref(err))
    if rc != 0:
        _raise("print_line", rc, err)


def hello(*, width=0, height=0, depth="truecolor", unicode=True, newline=True,
          reset_at_end=True) -> None:
    """零配置演示（对齐 api::hello）。"""
    o = _make_api_opts(width, height, depth, unicode, newline, reset_at_end)
    err = ctypes.c_void_p()
    rc = _c.tc_hello(ctypes.byref(o), ctypes.byref(err))
    if rc != 0:
        _raise("hello", rc, err)


# ── 输出控制（对齐 api::print）───────────────────────────────────────────────

def write_stdout(text) -> None:
    b = text.encode("utf-8") if isinstance(text, str) else bytes(text)
    rc = _c.tc_write_stdout(b, len(b), None)
    if rc != 0:
        raise TermchartError("write_stdout failed: status %d" % rc)


def write_stderr(text) -> None:
    b = text.encode("utf-8") if isinstance(text, str) else bytes(text)
    rc = _c.tc_write_stderr(b, len(b), None)
    if rc != 0:
        raise TermchartError("write_stderr failed: status %d" % rc)


def flush() -> None:
    err = ctypes.c_void_p()
    rc = _c.tc_flush_stdout(ctypes.byref(err))
    if rc != 0:
        _raise("flush", rc, err)


# ── AutoViz（对齐 api::auto_plot）────────────────────────────────────────────

def auto_plot(path) -> None:
    """读 CSV/JSONL 文件 → AutoViz 选图 → 出图到 stdout。"""
    err = ctypes.c_void_p()
    rc = _c.tc_auto_plot(str(path).encode("utf-8"), ctypes.byref(err))
    if rc != 0:
        _raise("auto_plot", rc, err)


def auto_plot_text(text) -> None:
    """内存 CSV/JSONL 文本 → AutoViz 选图 → 出图到 stdout（与 auto_plot 同引擎）。"""
    if not isinstance(text, str):
        raise TermchartError("auto_plot_text: text must be a string")
    err = ctypes.c_void_p()
    rc = _c.tc_auto_plot_text(text.encode("utf-8"), ctypes.byref(err))
    if rc != 0:
        _raise("auto_plot_text", rc, err)


class DataFrame:
    """容错 CSV 读取器（对齐 C++ ``data::DataFrame``）。

        df = DataFrame.parse("name,value\\nfoo,1\\nbar,2\\n")
        df.shape            # (2, 2)
        df.header(1)        # 'value'
        df.cell(0, 1)       # 1.0（数值）或原文（文本/缺失）
        df.cell_kind(0, 1)  # 'numeric' / 'text' / 'empty' / 'invalid'

    脏值保留原文并打质量标记，整表不报错——与 strict 的 io/csv 不同。
    """

    __slots__ = ("_h",)

    _KIND = ("empty", "numeric", "text", "invalid")
    _FLAG_NAMES = (
        (1, "comma"), (2, "accounting_negative"), (4, "percent"), (8, "full_width"),
        (16, "currency"), (32, "missing"), (64, "ragged"), (128, "unparsed"),
    )

    def __init__(self, handle):
        if not handle:
            raise TermchartError("null dataframe handle")
        self._h = handle

    @classmethod
    def parse(cls, text, *, delimiter=0, has_header=True, comment="#"):
        """内存 CSV 文本 → DataFrame；delimiter=0 自动嗅探，comment=None 关闭。"""
        if not isinstance(text, str):
            raise TermchartError("DataFrame.parse: text must be a string")
        o = _c.DataCsvOpts()
        o.delimiter = int(delimiter)
        o.has_header = 1 if has_header else 0
        o.comment = ord(comment) if comment else 0
        err = ctypes.c_void_p()
        h = _c.tc_data_parse_csv(text.encode("utf-8"), ctypes.byref(o), ctypes.byref(err))
        if not h:
            raise TermchartError("DataFrame.parse failed: %s" % (_err_text(err),))
        return cls(h)

    def __del__(self):
        if getattr(self, "_h", None):
            _c.tc_data_free(self._h)
            self._h = None

    def _alive(self):
        if not getattr(self, "_h", None):
            raise TermchartError("dataframe already closed")
        return self._h

    @property
    def shape(self):
        h = self._alive()
        return (_c.tc_data_row_count(h), _c.tc_data_col_count(h))

    def header(self, col):
        h = self._alive()
        return self._get_str(lambda buf, cap, need: _c.tc_data_header(
            h, int(col), buf, cap, need, None), "header")

    def cell_raw(self, row, col):
        h = self._alive()
        return self._get_str(lambda buf, cap, need: _c.tc_data_cell_raw(
            h, int(row), int(col), buf, cap, need, None), "cell_raw")

    @staticmethod
    def _get_str(call, what):
        need = ctypes.c_uint64(0)
        st = call(None, 0, ctypes.byref(need))
        if st != 0:
            raise TermchartError("%s failed (%d)" % (what, st))
        buf = ctypes.create_string_buffer(need.value + 1)
        got = ctypes.c_uint64(0)
        st = call(ctypes.cast(buf, ctypes.c_char_p), need.value, ctypes.byref(got))
        if st != 0:
            raise TermchartError("%s failed (%d)" % (what, st))
        return buf.value.decode("utf-8", "replace")

    def cell(self, row, col):
        """数值格返回 float，其余返回原文字符串。"""
        h = self._alive()
        out = ctypes.c_double()
        err = ctypes.c_void_p()
        rc = _c.tc_data_cell_number(h, int(row), int(col), ctypes.byref(out), ctypes.byref(err))
        if rc == 0:
            return out.value
        _c.tc_error_free(err)
        return self.cell_raw(row, col)

    def cell_kind(self, row, col):
        h = self._alive()
        kind = ctypes.c_int()
        err = ctypes.c_void_p()
        rc = _c.tc_data_cell_info(h, int(row), int(col), ctypes.byref(kind), None,
                                  ctypes.byref(err))
        if rc != 0:
            _raise("cell_kind", rc, err)
        return self._KIND[kind.value] if 0 <= kind.value < len(self._KIND) else "invalid"

    def cell_flags(self, row, col):
        h = self._alive()
        flags = ctypes.c_uint8()
        err = ctypes.c_void_p()
        rc = _c.tc_data_cell_info(h, int(row), int(col), None, ctypes.byref(flags),
                                  ctypes.byref(err))
        if rc != 0:
            _raise("cell_flags", rc, err)
        return tuple(n for bit, n in self._FLAG_NAMES if flags.value & bit)


# ── 句柄式图表（对齐 tc_chart_* 与 C++ chart::Chart 能力面）──────────────────────

_PROP = {
    "y_domain": _c.TC_PROP_Y_DOMAIN,
    "auto_domain": _c.TC_PROP_AUTO_DOMAIN,
    "include_zero": _c.TC_PROP_INCLUDE_ZERO,
    "emphasize_zero": _c.TC_PROP_EMPHASIZE_ZERO,
    "shade_negative": _c.TC_PROP_SHADE_NEGATIVE,
    "x_ticks": _c.TC_PROP_X_TICKS,
    "x_label": _c.TC_PROP_X_LABEL,
    "y_label": _c.TC_PROP_Y_LABEL,
}
_BOOL_PROPS = {"auto_domain", "include_zero", "emphasize_zero", "shade_negative"}
_STR_PROPS = {"x_label", "y_label"}


def kinds():
    """返回当前 C 库登记的全部图种名（列表，字典序）。

    图种会随版本增加；用本函数发现可用 kind，而不是硬编码列表。
    """
    need = ctypes.c_uint64(0)
    st = _c.tc_chart_kinds(None, 0, ctypes.byref(need), None)
    if st != 0:
        raise TermchartError("tc_chart_kinds failed (%d)" % (st,))
    buf = ctypes.create_string_buffer(need.value)
    got = ctypes.c_uint64(0)
    st = _c.tc_chart_kinds(buf, need.value, ctypes.byref(got), None)
    if st != 0:
        raise TermchartError("tc_chart_kinds failed (%d)" % (st,))
    return buf.raw[: got.value].decode("utf-8").split()


class Chart:
    """句柄式图表：多系列 / 值 / 矩阵 + 模型属性 + ANSI / SVG / PNG 输出。

    ``kind`` 为图种字符串（见 :func:`kinds`），经 C 侧通用入口 ``tc_chart_create``
    创建；同一对象可反复渲染（图在 C 侧缓存，只构建一次）。

        with Chart("line") as c:
            c.add_series([0,1,2,3], [1,4,9,16], name="sq", color=(255,140,0))
            c.set_prop("y_domain", (0, 18))
            print(c.ansi(width=60, height=16, title="demo"))

        with Chart("corr") as c:
            c.set_matrix([[1,.8],[.8,1]], row_labels=["a","b"])
            open("corr.svg", "w").write(c.to_svg())
    """

    def __init__(self, kind="line"):
        enc = kind.encode("utf-8") if isinstance(kind, str) else None
        if enc is None:
            raise TermchartError("kind must be a string")
        err = ctypes.c_void_p()
        self._h = _c.tc_chart_create(enc, ctypes.byref(err))
        if not self._h:
            raise TermchartError("unknown chart kind: %r" % (kind,))
        self.kind = kind

    # ── 生命周期 ──
    def close(self):
        if self._h:
            _c.tc_chart_free(self._h)
            self._h = None

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
        return False

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def _alive(self):
        if not self._h:
            raise TermchartError("chart already closed")
        return self._h

    # ── 数据 ──
    def add_series(self, x, y, *, name=None, color=None):
        self.add_many([{"x": x, "y": y, "name": name, "color": color}])

    def add_many(self, series):
        h = self._alive()
        n = len(series)
        if n == 0:
            return
        arr = (_c.Series * n)()
        keep = []  # 保证 C 调用期间数组 / 字符串不被 GC
        for i, item in enumerate(series):
            xs = (ctypes.c_double * len(item["x"]))(*item["x"])
            ys = (ctypes.c_double * len(item["y"]))(*item["y"])
            keep.extend([xs, ys])
            nb = item["name"].encode("utf-8") if item.get("name") else None
            if nb is not None:
                keep.append(nb)
            arr[i].name = nb
            _fill_color(arr[i].color, item.get("color"))
            arr[i].x = xs
            arr[i].y = ys
            arr[i].n = len(item["x"])
        err = ctypes.c_void_p()
        st = _c.tc_chart_add_series(
            h, ctypes.cast(arr, ctypes.c_void_p), n, ctypes.byref(err)
        )
        if st != 0:
            _raise("add_series", st, err)

    def add_points(self, x, y):
        """追加单条无名系列（与库内 add_* 约定一致）。"""
        self.add_series(x, y)

    def add_values(self, values, labels=None):
        """喂入"值 + 类别标签"（柱状 / 饼图 / 迷你图通用）。"""
        h = self._alive()
        n = len(values)
        vals = (ctypes.c_double * n)(*values)
        keep = [vals]
        labs = None
        if labels is not None:
            ls = [l.encode("utf-8") if l else None for l in labels]
            keep.append(ls)
            labs = (ctypes.c_char_p * len(ls))(*ls)
        err = ctypes.c_void_p()
        st = _c.tc_chart_set_values(h, vals, n, labs, len(labels) if labels else 0, ctypes.byref(err))
        if st != 0:
            _raise("add_values", st, err)

    def set_matrix(self, rows, *, row_labels=None, col_labels=None):
        """喂入二维矩阵（contour / corr / mosaic / violin / pca / parallel 等）。

        ``rows`` 为行优先的二维序列（list of list，行数/列数须一致）。
        """
        h = self._alive()
        nrow = len(rows)
        if nrow == 0:
            return
        ncol = len(rows[0])
        keep = []
        row_ptrs = []
        for r in rows:
            if len(r) != ncol:
                raise TermchartError("set_matrix: ragged rows")
            arr = (ctypes.c_double * ncol)(*r)
            keep.append(arr)
            row_ptrs.append(ctypes.cast(arr, ctypes.POINTER(ctypes.c_double)))
        ptrs = (ctypes.POINTER(ctypes.c_double) * nrow)(*row_ptrs)

        def enc_labels(labels):
            if labels is None:
                return None, 0, []
            ls = [x.encode("utf-8") if x else None for x in labels]
            return (ctypes.c_char_p * len(ls))(*ls), len(ls), ls

        rl, nrl, keep_rl = enc_labels(row_labels)
        cl, ncl, keep_cl = enc_labels(col_labels)
        keep.extend(keep_rl)
        keep.extend(keep_cl)
        err = ctypes.c_void_p()
        st = _c.tc_chart_set_matrix(
            h, ptrs, nrow, ncol,
            rl, nrl, cl, ncl, ctypes.byref(err),
        )
        if st != 0:
            _raise("set_matrix", st, err)

    # ── 属性 ──
    def set_prop(self, prop, value=None):
        h = self._alive()
        pid = _PROP.get(prop, prop if isinstance(prop, int) else None)
        if pid is None:
            raise TermchartError("unknown chart property: %r" % (prop,))
        v = _c.Value()
        keep = []
        if value is None:
            v.kind = _c.TC_VAL_NONE
        elif prop in _BOOL_PROPS:
            v.kind = _c.TC_VAL_BOOL
            v.i = 1 if value else 0
        elif prop in _STR_PROPS:
            if not isinstance(value, str):
                raise TermchartError("property %r expects a string" % (prop,))
            enc = value.encode("utf-8")
            keep.append(enc)
            v.kind = _c.TC_VAL_STRING
            v.s = enc
        else:
            arr = (ctypes.c_double * len(value))(*value)
            keep.append(arr)
            v.kind = _c.TC_VAL_DOUBLE_ARRAY
            v.p = arr
            v.n = len(value)
        err = ctypes.c_void_p()
        st = _c.tc_chart_set_prop(h, pid, ctypes.byref(v), ctypes.byref(err))
        if st != 0:
            _raise("set_prop", st, err)

    # ── 其余数据形状 ──
    def set_scalar(self, value):
        """单值图种（gauge / progress_ring / star_rating / thermometer / status_light）。"""
        st = _c.tc_chart_set_scalar(self._alive(), float(value), None)
        if st != 0:
            _raise("set_scalar", st, None)

    def set_tree(self, nodes):
        """层次图（treemap/sunburst/icicle/dendrogram/pack/tree_view）。

        ``nodes`` 为 ``(parent_index, name, value)`` 序列，parent=-1 表示根。
        """
        n = len(nodes)
        if n == 0:
            return
        parents = (ctypes.c_int * n)(*[int(x[0]) for x in nodes])
        names_buf = [x[1].encode("utf-8") if x[1] else None for x in nodes]
        names = (ctypes.c_char_p * n)(*names_buf)
        vals = (ctypes.c_double * n)(*[float(x[2]) for x in nodes])
        st = _c.tc_chart_set_tree(self._alive(), parents, names, vals, n, None)
        if st != 0:
            _raise("set_tree", st, None)

    def set_text(self, rows, blob=None):
        """文本图种（table / gauge_kpi / geo_map）。

        ``rows`` 为行优先的字符串二维序列；``blob`` 为 GeoJSON 文本（geo_map 用）。
        """
        rbuf = []
        for row in rows:
            cells = [str(c).encode("utf-8") for c in row]
            rbuf.append((ctypes.c_char_p * len(cells))(*cells))
        ptrs = (ctypes.POINTER(ctypes.c_char_p) * len(rbuf))(
            *(ctypes.cast(r, ctypes.POINTER(ctypes.c_char_p)) for r in rbuf)
        )
        cc = (ctypes.c_uint64 * len(rbuf))(*[len(r) for r in rows])
        st = _c.tc_chart_set_text(self._alive(), ptrs, len(rbuf), cc,
                                  blob.encode("utf-8") if blob else None, None)
        if st != 0:
            _raise("set_text", st, None)

    def set_regions(self, regions, cells=None):
        """区域地理图种（choropleth / dot_density / geo_map）。

        ``regions`` 为 ``(name, value, [(x,y),...])`` 序列；cells 可省略（geo_map 只需 name/value）。
        """
        n = len(regions)
        if n == 0:
            return
        names_buf = [r[0].encode("utf-8") if r[0] else None for r in regions]
        names = (ctypes.c_char_p * n)(*names_buf)
        vals = (ctypes.c_double * n)(*[float(r[1]) for r in regions])
        all_cells = []
        starts, counts = [], []
        for r in regions:
            starts.append(len(all_cells))
            cs = r[2] if len(r) > 2 else []
            counts.append(len(cs))
            all_cells.extend(cs)
        nc = len(all_cells)
        cx = (ctypes.c_double * nc)(*[c[0] for c in all_cells]) if nc else None
        cy = (ctypes.c_double * nc)(*[c[1] for c in all_cells]) if nc else None
        st = (ctypes.c_uint64 * n)(*starts)
        ct = (ctypes.c_uint64 * n)(*counts)
        stt = _c.tc_chart_set_regions(self._alive(), names, vals, n, cx, cy, st, ct, nc, None)
        if stt != 0:
            _raise("set_regions", stt, None)

    # ── 输出 ──
    def _opts(self, width, height, title, show_legend, depth):
        return _plot_opts(width, height, title, show_legend, depth)

    def ansi(self, *, width=0, height=0, title=None, show_legend=False, depth="truecolor"):
        h = self._alive()
        o = self._opts(width, height, title, show_legend, depth)
        out = ctypes.c_char_p()
        err = ctypes.c_void_p()
        st = _c.tc_chart_render_ansi(h, ctypes.byref(o), ctypes.byref(out), ctypes.byref(err))
        if st != 0:
            _raise("render", st, err)
        r = out.value
        _c.tc_free_string(out)
        return r.decode("utf-8") if isinstance(r, (bytes, bytearray)) else r

    def print(self, *, width=0, height=0, title=None, show_legend=False, depth="truecolor"):
        h = self._alive()
        o = self._opts(width, height, title, show_legend, depth)
        err = ctypes.c_void_p()
        st = _c.tc_chart_print(h, ctypes.byref(o), ctypes.byref(err))
        if st != 0:
            _raise("print", st, err)

    def to_svg(self, path=None, *, width=0, height=0, title=None, show_legend=False):
        h = self._alive()
        o = self._opts(width, height, title, show_legend, "truecolor")
        need = ctypes.c_uint64(0)
        err = ctypes.c_void_p()
        st = _c.tc_chart_to_svg(
            h, ctypes.byref(o), None, 0, ctypes.byref(need), ctypes.byref(err)
        )
        if st != 0:
            _raise("to_svg", st, err)
        buf = ctypes.create_string_buffer(need.value)
        got = ctypes.c_uint64(0)
        st = _c.tc_chart_to_svg(
            h,
            ctypes.byref(o),
            ctypes.cast(buf, ctypes.c_char_p),
            need.value,
            ctypes.byref(got),
            ctypes.byref(err),
        )
        if st != 0:
            _raise("to_svg", st, err)
        text = bytes(buf.raw[: got.value]).decode("utf-8")
        if path is not None:
            with open(str(path), "wb") as fh:
                fh.write(text.encode("utf-8"))
        return text

    def to_png(self, path=None, *, width=0, height=0, title=None, show_legend=False):
        h = self._alive()
        o = self._opts(width, height, title, show_legend, "truecolor")
        need = ctypes.c_uint64(0)
        err = ctypes.c_void_p()
        st = _c.tc_chart_to_png(
            h, ctypes.byref(o), None, 0, ctypes.byref(need), ctypes.byref(err)
        )
        if st != 0:
            _raise("to_png", st, err)
        buf = ctypes.create_string_buffer(need.value)
        got = ctypes.c_uint64(0)
        st = _c.tc_chart_to_png(
            h,
            ctypes.byref(o),
            ctypes.cast(buf, ctypes.c_char_p),
            need.value,
            ctypes.byref(got),
            ctypes.byref(err),
        )
        if st != 0:
            _raise("to_png", st, err)
        data = bytes(buf.raw[: got.value])
        if path is not None:
            with open(str(path), "wb") as fh:
                fh.write(data)
        return data

    def to_protocol(self, kind="auto", *, width=0, height=0, title=None,
                    show_legend=False, depth="truecolor",
                    cell_width=10, cell_height=20, rle=True) -> bytes:
        """直接编码成终端内联图片字节流（sixel / kitty / iterm2 / 半块）。

        等价于「``to_png()`` 再 ``Image.decode()`` 再 ``to_protocol``」，但像素只在
        C 侧内存里流转一次。``kind="auto"`` 走与 C++ 相同的探测降级链。

            print(chart.to_protocol(), end="")
        """
        h = self._alive()
        o = self._opts(width, height, title, show_legend, depth)
        pk = _proto_kind(kind)
        po = _protocol_opts(cell_width, cell_height, rle)
        return _two_phase_bytes(
            lambda buf, cap, need, err: _c.tc_chart_to_protocol(
                h, pk, ctypes.byref(o), ctypes.byref(po), buf, cap, need, err
            ),
            "chart to_protocol",
        )


# ── 图像（tc_image_* ↔ image::Bitmap）────────────────────────────────────────
#
# 像素操作全部落在 C 侧同名成员（fill / fill_rect / blit_over / scaled_to /
# with_alpha / without_alpha），Python 只搬字节。这就是"Python 效果与 C++
# 逐字节一致"的依据：没有第二套栅格化、没有第二套混合公式。

def _err_text(err) -> str:
    if not err:
        return ""
    buf = ctypes.create_string_buffer(512)
    _c.tc_error_message(err, buf, 512)
    text = buf.value.decode("utf-8", "replace")
    _c.tc_error_free(err)
    return text


class Image:
    """图像句柄：对应 C++ ``image::Bitmap``（3/4 通道、行紧致）。

        img = Image.create(64, 64)
        img.fill(20, 24, 32)
        img.fill_rect(8, 8, 32, 32, 255, 140, 0)
        img.save("out.png")

        src = Image.load("logo.png")
        base.blit_over(src, 100, 50)      # 源 alpha 做 source-over
    """

    __slots__ = ("_h",)

    def __init__(self, handle):
        if not handle:
            raise TermchartError("null image handle")
        self._h = handle

    # ── 构造 ──
    @classmethod
    def create(cls, width, height, channels=4):
        err = ctypes.c_void_p()
        h = _c.tc_image_create(int(width), int(height), int(channels), ctypes.byref(err))
        if not h:
            raise TermchartError("image create failed: %s" % (_err_text(err),))
        return cls(h)

    @classmethod
    def load(cls, path):
        err = ctypes.c_void_p()
        h = _c.tc_image_load(str(path).encode("utf-8"), ctypes.byref(err))
        if not h:
            raise TermchartError("image load failed: %s" % (_err_text(err),))
        return cls(h)

    @classmethod
    def decode(cls, data):
        buf = bytes(data)
        if not buf:
            raise TermchartError("image decode: empty input")
        arr = (ctypes.c_uint8 * len(buf)).from_buffer_copy(buf)
        err = ctypes.c_void_p()
        h = _c.tc_image_decode(arr, len(buf), ctypes.byref(err))
        if not h:
            raise TermchartError("image decode failed: %s" % (_err_text(err),))
        return cls(h)

    # ── 生命周期 ──
    def close(self):
        if self._h:
            _c.tc_image_free(self._h)
            self._h = None

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
        return False

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def _alive(self):
        if not self._h:
            raise TermchartError("image already closed")
        return self._h

    # ── 尺寸 ──
    @property
    def width(self) -> int:
        return int(_c.tc_image_width(self._alive()))

    @property
    def height(self) -> int:
        return int(_c.tc_image_height(self._alive()))

    @property
    def channels(self) -> int:
        return int(_c.tc_image_channels(self._alive()))

    def __repr__(self):
        if not self._h:
            return "<termchart.Image closed>"
        return "<termchart.Image %dx%d ch=%d>" % (self.width, self.height, self.channels)

    # ── 像素 ──
    def pixels(self) -> bytes:
        """行紧致 ``width*height*channels`` 原始字节。"""
        h = self._alive()
        need = ctypes.c_uint64(0)
        st = _c.tc_image_pixels(h, None, 0, ctypes.byref(need), None)
        if st != _c.TC_ERR_BUFFER_TOO_SMALL and st != 0:
            raise TermchartError("image pixels failed (%d)" % (st,))
        buf = ctypes.create_string_buffer(need.value)
        got = ctypes.c_uint64(0)
        err = ctypes.c_void_p()
        st = _c.tc_image_pixels(
            h, ctypes.cast(buf, ctypes.POINTER(ctypes.c_uint8)), need.value,
            ctypes.byref(got), ctypes.byref(err),
        )
        if st != 0:
            raise TermchartError("image pixels failed: %s" % (_err_text(err),))
        return bytes(buf.raw[: got.value])

    def numpy(self):
        """返回 ``(H, W, C)`` 的 uint8 数组（拷贝；需要 numpy）。"""
        import numpy as np  # 延迟导入：不用图像功能就不必装 numpy

        return np.frombuffer(self.pixels(), dtype=np.uint8).reshape(
            self.height, self.width, self.channels
        ).copy()

    # ── 派生（返回新图，源不变；与 C++ 返回 Bitmap 的成员同构）──
    def scaled_to(self, width, height):
        err = ctypes.c_void_p()
        h = _c.tc_image_scaled(self._alive(), int(width), int(height), ctypes.byref(err))
        if not h:
            raise TermchartError("image scaled_to failed: %s" % (_err_text(err),))
        return Image(h)

    def with_alpha(self, alpha=255):
        err = ctypes.c_void_p()
        h = _c.tc_image_with_alpha(self._alive(), int(alpha), ctypes.byref(err))
        if not h:
            raise TermchartError("image with_alpha failed: %s" % (_err_text(err),))
        return Image(h)

    def without_alpha(self):
        err = ctypes.c_void_p()
        h = _c.tc_image_without_alpha(self._alive(), ctypes.byref(err))
        if not h:
            raise TermchartError("image without_alpha failed: %s" % (_err_text(err),))
        return Image(h)

    # ── 原地绘制 ──
    def fill(self, r, g, b, a=255):
        _c.tc_image_fill(self._alive(), int(r), int(g), int(b), int(a))
        return self

    def fill_rect(self, x, y, w, h, r, g, b, a=255):
        _c.tc_image_fill_rect(
            self._alive(), int(x), int(y), int(w), int(h), int(r), int(g), int(b), int(a)
        )
        return self

    def blit_over(self, src, dx, dy):
        if not isinstance(src, Image):
            raise TermchartError("blit_over: src must be a termchart Image")
        _c.tc_image_blit_over(self._alive(), src._alive(), int(dx), int(dy))
        return self

    # ── 编码 ──
    def encode(self, format="png", quality=85) -> bytes:
        """编码成字节流（png / jpeg / gif / webp）。无损格式忽略 quality。"""
        h = self._alive()
        fmt = format.encode("utf-8")
        need = ctypes.c_uint64(0)
        st = _c.tc_image_encode(h, fmt, int(quality), None, 0, ctypes.byref(need), None)
        if st != _c.TC_ERR_BUFFER_TOO_SMALL:
            raise TermchartError("image encode failed (%d)" % (st,))
        buf = ctypes.create_string_buffer(need.value)
        got = ctypes.c_uint64(0)
        err = ctypes.c_void_p()
        st = _c.tc_image_encode(
            h, fmt, int(quality), buf, need.value, ctypes.byref(got), ctypes.byref(err)
        )
        if st != 0:
            raise TermchartError("image encode failed: %s" % (_err_text(err),))
        return bytes(buf.raw[: got.value])

    def save(self, path, format=None, quality=85) -> None:
        """写文件；format 省略时按路径后缀推断（显式传参时不看后缀）。"""
        fmt = (format or "").encode("utf-8")
        err = ctypes.c_void_p()
        st = _c.tc_image_save(
            self._alive(), str(path).encode("utf-8"), fmt or None, int(quality),
            ctypes.byref(err),
        )
        if st != 0:
            raise TermchartError("image save failed: %s" % (_err_text(err),))

    # ── 终端协议 ──
    def to_protocol(self, kind="auto", *, cell_width=10, cell_height=20,
                    rle=True) -> bytes:
        """位图 → 终端内联图片字节流（sixel / kitty / iterm2 / 半块）。

        ``cell_width`` / ``cell_height`` 只在半块降级时用到（其余协议按真实像素
        走）；``rle`` 只影响 sixel 的行程压缩。
        """
        pk = _proto_kind(kind)
        po = _protocol_opts(cell_width, cell_height, rle)
        return _two_phase_bytes(
            lambda buf, cap, need, err: _c.tc_image_to_protocol(
                self._alive(), pk, ctypes.byref(po), buf, cap, need, err
            ),
            "image to_protocol",
        )


def load_image(path):
    """解码文件为 Image（PNG/BMP 零依赖；JPEG/GIF/WebP 视构建开关）。"""
    return Image.load(path)


def decode_image(data):
    """解码内存字节为 Image。"""
    return Image.decode(data)


# ── 关系图 / 示意图（tc_graph_* ↔ graph::diagrams）────────────────────────────
#
# 布局、正交路由、箭头与标签避让全在 C++ 里；这里只搬节点 / 边 / 属性与字节。
# 因此 Python 画出来的流程图与 C++ examples/14 逐字节相同。
#
# 属性通道与 tc_chart_set_prop 同构：kind 不支持的属性由 C 侧返回
# TC_ERR_UNSUPPORTED（fail-loud），Python 不做"猜一个相近的"。

# 与 termchart_capi.h 的 kind 登记表一致；仅用于文档/校验，不参与分派。
GRAPH_KINDS = (
    "flowchart", "state_machine", "mindmap", "orgchart",
    "dag", "dependency", "er_diagram", "knowledge_graph",
    "social_network", "uml_activity", "uml_class", "uml_usecase",
)

_GRAPH_BOOL_PROPS = {"edge_labels", "arrows", "node_border", "mark_cycles"}
_GRAPH_NUM_PROPS = {"min_width", "min_height", "h_gap", "v_gap"}


class Graph:
    """关系图 / 示意图句柄（对齐 ``graph::diagrams`` 的 12 个入口）。

    ``kind`` 取 :data:`GRAPH_KINDS` 里的名字；``kind=None`` 等价 ``"flowchart"``。

        with Graph("flowchart") as g:
            a = g.add_node("parse")
            b = g.add_node("render")
            g.add_edge(a, b, "emit")
            print(g.ansi(width=60))

        g = graph_from_text("graph TD; A-->B; B-->C", format="mermaid")
        g.to_png("flow.png")

    ``from_text`` 的 ``format`` 传 ``None`` 时按 dsl 注册表逐个方言尝试（mermaid /
    dot / d2 / plantuml …），第一个解析成功者胜出——与 C++ 一致。
    """

    def __init__(self, kind="flowchart"):
        enc = kind.encode("utf-8") if isinstance(kind, str) and kind else None
        err = ctypes.c_void_p()
        self._h = _c.tc_graph_create(enc, ctypes.byref(err))
        if not self._h:
            raise TermchartError("graph create failed: %s" % (_err_text(err),))
        self.kind = kind or "flowchart"

    @classmethod
    def from_text(cls, text, format=None, kind=None):
        """文本方言 → 图（mermaid / dot / d2 / plantuml / 注册表里的其余名字）。"""
        if not isinstance(text, str):
            raise TermchartError("graph_from_text: text must be a string")
        enc_text = text.encode("utf-8")
        enc_fmt = format.encode("utf-8") if format else None
        enc_kind = kind.encode("utf-8") if kind else None
        err = ctypes.c_void_p()
        h = _c.tc_graph_from_text(enc_text, enc_fmt, enc_kind, ctypes.byref(err))
        if not h:
            raise TermchartError("graph_from_text failed: %s" % (_err_text(err),))
        g = cls.__new__(cls)
        g._h = h
        g.kind = kind or "flowchart"
        return g

    # ── 生命周期 ──
    def close(self):
        if self._h:
            _c.tc_graph_free(self._h)
            self._h = None

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
        return False

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def _alive(self):
        if not self._h:
            raise TermchartError("graph already closed")
        return self._h

    def __repr__(self):
        if not self._h:
            return "<termchart.Graph closed>"
        return "<termchart.Graph %s nodes=%d edges=%d>" % (
            self.kind, self.node_count, self.edge_count,
        )

    # ── 建图 ──
    def add_node(self, label="") -> int:
        """加节点，返回节点 id（从 1 开始；0 只用于失败哨兵）。"""
        ident = _c.tc_graph_add_node(
            self._alive(), label.encode("utf-8") if label else None
        )
        if ident == 0:
            raise TermchartError("add_node failed (graph closed?)")
        return int(ident)

    def add_edge(self, src, dst, label=None):
        """加边；端点不存在时 C 侧报 ``TC_ERR_INVALID_ARG``（这里转成异常）。"""
        err = ctypes.c_void_p()
        st = _c.tc_graph_add_edge(
            self._alive(), int(src), int(dst),
            label.encode("utf-8") if label else None,
            ctypes.byref(err),
        )
        if st != 0:
            _raise("add_edge", st, err)
        return self

    def set_node_attr(self, node, key, value=""):
        """图种专属标记（如状态机的 ``key="kind", value="initial"``）。"""
        err = ctypes.c_void_p()
        st = _c.tc_graph_set_node_attr(
            self._alive(), int(node), str(key).encode("utf-8"),
            value.encode("utf-8") if isinstance(value, str) else None,
            ctypes.byref(err),
        )
        if st != 0:
            _raise("set_node_attr", st, err)
        return self

    def set_edge_attr(self, src, dst, key, value=""):
        err = ctypes.c_void_p()
        st = _c.tc_graph_set_edge_attr(
            self._alive(), int(src), int(dst), str(key).encode("utf-8"),
            value.encode("utf-8") if isinstance(value, str) else None,
            ctypes.byref(err),
        )
        if st != 0:
            _raise("set_edge_attr", st, err)
        return self

    # ── 外观（属性通道）──
    def set_prop(self, prop, value=None):
        """通用属性；``value=None`` 表示清除（回到图种默认）。

        ``shape`` / ``route`` 可用字符串（box/rounded/diamond/ellipse/stadium/
        hexagon/cylinder/icon、straight/orthogonal），也可直接用整数枚举值。
        """
        h = self._alive()
        pid = _GRAPH_PROP.get(prop, prop if isinstance(prop, int) else None)
        if pid is None:
            raise TermchartError("unknown graph property: %r" % (prop,))

        v = _c.Value()
        keep = []
        if value is None:
            v.kind = _c.TC_VAL_NONE
        elif prop in _GRAPH_BOOL_PROPS:
            v.kind = _c.TC_VAL_BOOL
            v.i = 1 if value else 0
        elif prop == "shape":
            if isinstance(value, str):
                if value not in _SHAPE:
                    raise TermchartError(
                        "unknown shape %r (%s)" % (value, "/".join(_SHAPE))
                    )
                value = _SHAPE[value]
            v.kind = _c.TC_VAL_INT
            v.i = int(value)
        elif prop == "route":
            if isinstance(value, str):
                if value not in _ROUTE:
                    raise TermchartError(
                        "unknown route %r (straight/orthogonal)" % (value,)
                    )
                value = _ROUTE[value]
            v.kind = _c.TC_VAL_INT
            v.i = int(value)
        elif prop in _GRAPH_NUM_PROPS:
            v.kind = _c.TC_VAL_DOUBLE
            v.d = float(value)
        elif isinstance(value, bool):
            v.kind = _c.TC_VAL_BOOL
            v.i = 1 if value else 0
        elif isinstance(value, int):
            v.kind = _c.TC_VAL_INT
            v.i = value
        elif isinstance(value, float):
            v.kind = _c.TC_VAL_DOUBLE
            v.d = value
        elif isinstance(value, str):
            enc = value.encode("utf-8")
            keep.append(enc)
            v.kind = _c.TC_VAL_STRING
            v.s = enc
        else:
            raise TermchartError("graph property %r: unsupported value %r" % (prop, value))

        err = ctypes.c_void_p()
        st = _c.tc_graph_set_prop(h, pid, ctypes.byref(v), ctypes.byref(err))
        if st != 0:
            _raise("set_prop", st, err)
        return self

    def set_style(self, *, node_fg=None, node_bg=None, node_attrs=0,
                  edge_fg=None, edge_bg=None, edge_attrs=0):
        """节点 / 边主色；两端都可省略（省略 = 保持 C++ 默认配色）。"""
        h = self._alive()
        ns = None
        if node_fg is not None or node_bg is not None or node_attrs:
            ns = _make_style(node_fg, node_bg, node_attrs)
        es = None
        if edge_fg is not None or edge_bg is not None or edge_attrs:
            es = _make_style(edge_fg, edge_bg, edge_attrs)
        err = ctypes.c_void_p()
        st = _c.tc_graph_set_style(
            h,
            ctypes.byref(ns) if ns is not None else None,
            ctypes.byref(es) if es is not None else None,
            ctypes.byref(err),
        )
        if st != 0:
            _raise("set_style", st, err)
        return self

    # ── 规模 ──
    @property
    def node_count(self) -> int:
        return int(_c.tc_graph_node_count(self._alive()))

    @property
    def edge_count(self) -> int:
        return int(_c.tc_graph_edge_count(self._alive()))

    # ── 出图 ──
    def ansi(self, *, width=0, height=0, title=None, show_legend=False,
             depth="truecolor") -> str:
        """渲染成 ANSI 字符串；``width`` / ``height`` 为 0 时按包围盒自适应。"""
        h = self._alive()
        o = _plot_opts(width, height, title, show_legend, depth)
        return _two_phase_str(
            lambda buf, cap, need, err: _c.tc_graph_render_ansi(
                h, ctypes.byref(o), buf, cap, need, err
            ),
            "graph ansi",
        )

    def print(self, *, width=0, height=0, title=None, show_legend=False,
              depth="truecolor") -> None:
        """渲染并直接写 stdout（对齐 ``tc_graph_print``）。"""
        h = self._alive()
        o = _plot_opts(width, height, title, show_legend, depth)
        err = ctypes.c_void_p()
        st = _c.tc_graph_print(h, ctypes.byref(o), ctypes.byref(err))
        if st != 0:
            _raise("graph print", st, err)

    def to_png(self, path=None, *, width=0, height=0, title=None,
               show_legend=False, depth="truecolor") -> bytes:
        """渲染成 PNG 字节流（逐格像素化，与终端贴图共用同一位图）。"""
        h = self._alive()
        o = _plot_opts(width, height, title, show_legend, depth)
        data = _two_phase_bytes(
            lambda buf, cap, need, err: _c.tc_graph_to_png(
                h, ctypes.byref(o), buf, cap, need, err
            ),
            "graph to_png",
        )
        if path is not None:
            with open(str(path), "wb") as fh:
                fh.write(data)
        return data

    def to_protocol(self, kind="auto", *, width=0, height=0, title=None,
                    show_legend=False, depth="truecolor",
                    cell_width=10, cell_height=20, rle=True) -> bytes:
        """图 → 终端内联图片字节流（sixel / kitty / iterm2 / 半块）。"""
        h = self._alive()
        o = _plot_opts(width, height, title, show_legend, depth)
        pk = _proto_kind(kind)
        po = _protocol_opts(cell_width, cell_height, rle)
        return _two_phase_bytes(
            lambda buf, cap, need, err: _c.tc_graph_to_protocol(
                h, pk, ctypes.byref(o), ctypes.byref(po), buf, cap, need, err
            ),
            "graph to_protocol",
        )


def graph_from_text(text, format=None, kind=None) -> Graph:
    """文本方言 → Graph（``format=None`` 时按注册表逐个方言尝试）。"""
    return Graph.from_text(text, format=format, kind=kind)


# ── 文档（tc_doc_* ↔ document::Document + backend::structured）──────────────────
#
# 文档是"块序列"：标题 / 段落 / 代码 / 表格 / 图表。排版与转义全在 C++ 的
# backend::structured 里，因此导出的 HTML / Markdown 与 examples/36 逐字节相同。

_DOC_FORMAT_ALIAS = {
    "md": "markdown",
    "markdown": "markdown",
    "text": "plain",
    "txt": "plain",
    "plain": "plain",
    "html": "html",
}

_DOC_SUFFIX_FORMAT = {
    ".md": "markdown",
    ".markdown": "markdown",
    ".txt": "plain",
    ".html": "html",
    ".htm": "html",
}


class Document:
    """文档块序列 → HTML / Markdown / 纯文本。

        doc = Document()
        doc.add_heading("季度报告", 1)
        doc.add_paragraph("见下表。")
        doc.add_table(["月份", "销量"], [["1月", "120"], ["2月", "168"]])
        doc.add_chart(chart, width=60, height=20)
        open("report.html", "w", encoding="utf-8").write(doc.export("html"))
    """

    def __init__(self):
        err = ctypes.c_void_p()
        self._h = _c.tc_doc_create(ctypes.byref(err))
        if not self._h:
            raise TermchartError("document create failed: %s" % (_err_text(err),))

    # ── 生命周期 ──
    def close(self):
        if self._h:
            _c.tc_doc_free(self._h)
            self._h = None

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
        return False

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def _alive(self):
        if not self._h:
            raise TermchartError("document already closed")
        return self._h

    def __repr__(self):
        return "<termchart.Document %s>" % ("closed" if not self._h else "open")

    # ── 块 ──
    def add_heading(self, text, level=1):
        err = ctypes.c_void_p()
        st = _c.tc_doc_add_heading(
            self._alive(), str(text).encode("utf-8"), int(level), ctypes.byref(err)
        )
        if st != 0:
            _raise("add_heading", st, err)
        return self

    def add_paragraph(self, text):
        err = ctypes.c_void_p()
        st = _c.tc_doc_add_paragraph(
            self._alive(), str(text).encode("utf-8"), ctypes.byref(err)
        )
        if st != 0:
            _raise("add_paragraph", st, err)
        return self

    def add_rich_paragraph(self, spans):
        """富文本段落：``spans`` 为 ``[(text, style_or_None), ...]``。"""
        arr = (_c.Span * len(spans))()
        keep = []  # 持有 buffer 引用，防止调用期间 text 指针悬空
        for i, item in enumerate(spans):
            text, style = (item if isinstance(item, (tuple, list)) else (item, None))
            buf = ctypes.create_string_buffer(str(text).encode("utf-8"))
            keep.append(buf)
            arr[i].text = ctypes.cast(buf, ctypes.c_char_p)
            arr[i].style = style if style is not None else _c.Style()
        err = ctypes.c_void_p()
        st = _c.tc_doc_add_rich_paragraph(self._alive(), arr, len(arr), ctypes.byref(err))
        if st != 0:
            _raise("add_rich_paragraph", st, err)
        return self

    def add_code(self, text, language=None):
        err = ctypes.c_void_p()
        st = _c.tc_doc_add_code(
            self._alive(), str(text).encode("utf-8"),
            language.encode("utf-8") if language else None,
            ctypes.byref(err),
        )
        if st != 0:
            _raise("add_code", st, err)
        return self

    def add_table(self, header, rows):
        """``header`` 为列名序列，``rows`` 为二维序列（缺列自动补空串）。

        C 侧按 ``n_cols`` 定长读取每行，所以这里先把行补齐/截断到 ``n_cols``，
        避免绑定层把越界指针交给 C。
        """
        h = self._alive()
        n_cols = len(header) if header is not None else 0
        if n_cols == 0:
            raise TermchartError("add_table: header needs at least one column")

        keep = []
        cols = [("" if c is None else str(c)).encode("utf-8") for c in header]
        keep.append(cols)
        hdr = (ctypes.c_char_p * n_cols)(*cols)

        rbuf = []
        for row in rows or ():
            cells = ["" if c is None else str(c) for c in row]
            cells = (cells + [""] * n_cols)[:n_cols]
            enc = [c.encode("utf-8") for c in cells]
            arr = (ctypes.c_char_p * n_cols)(*enc)
            rbuf.append(arr)
            keep.append(arr)   # arr 持有 bytes 指针，必须活到 C 调用返回
            keep.append(enc)
        rptrs = (ctypes.POINTER(ctypes.c_char_p) * len(rbuf))(
            *(ctypes.cast(r, ctypes.POINTER(ctypes.c_char_p)) for r in rbuf)
        )

        err = ctypes.c_void_p()
        st = _c.tc_doc_add_table(
            h, hdr, n_cols, rptrs, len(rbuf), ctypes.byref(err)
        )
        if st != 0:
            _raise("add_table", st, err)
        return self

    def add_chart(self, chart, width=0, height=0):
        """把 Chart 作为一张矢量图嵌进文档（走 ``api::chart_to_scene``）。"""
        if not isinstance(chart, Chart):
            raise TermchartError("add_chart: chart must be a termchart Chart")
        err = ctypes.c_void_p()
        st = _c.tc_doc_add_chart(
            self._alive(), chart._alive(), int(width), int(height), ctypes.byref(err)
        )
        if st != 0:
            _raise("add_chart", st, err)
        return self

    # ── 导出 ──
    def export(self, format="html", *, title=None, css_class=None) -> str:
        """一次性导出文本：``html`` / ``markdown`` / ``plain``（别名 md / txt）。"""
        key = _DOC_FORMAT_ALIAS.get((format or "html").lower() if isinstance(format, str) else format)
        if key is None:
            raise TermchartError(
                "unknown document format %r (html/markdown/plain)" % (format,)
            )
        h = self._alive()
        fmt = key.encode("utf-8")
        ttl = title.encode("utf-8") if title else None
        css = css_class.encode("utf-8") if css_class else None
        return _two_phase_str(
            lambda buf, cap, need, err: _c.tc_doc_export(
                h, fmt, ttl, css, buf, cap, need, err
            ),
            "doc export",
        )

    def save(self, path, format=None, *, title=None, css_class=None) -> None:
        """导出并写文件；``format`` 省略时按后缀推断（.md/.txt/.html，其余 html）。"""
        if format is None:
            suffix = str(path).lower()
            format = "html"
            for ext, name in _DOC_SUFFIX_FORMAT.items():
                if suffix.endswith(ext):
                    format = name
                    break
        key = _DOC_FORMAT_ALIAS.get((format or "html").lower())
        if key is None:
            raise TermchartError(
                "unknown document format %r (html/markdown/plain)" % (format,)
            )
        err = ctypes.c_void_p()
        st = _c.tc_doc_save(
            self._alive(), str(path).encode("utf-8"), key.encode("utf-8"),
            title.encode("utf-8") if title else None,
            css_class.encode("utf-8") if css_class else None,
            ctypes.byref(err),
        )
        if st != 0:
            _raise("doc save", st, err)


def document() -> Document:
    """新建空文档（等价 ``Document()``）。"""
    return Document()


# ── 终端 ────────────────────────────────────────────────────────────────────

_OUT_FORMAT = {
    "ansi": _c.TC_OUT_ANSI, "text": _c.TC_OUT_TEXT,
    "html_pre": _c.TC_OUT_HTML_PRE, "json": _c.TC_OUT_JSON,
    "png": _c.TC_OUT_PNG, "svg": _c.TC_OUT_SVG,
    "protocol": _c.TC_OUT_PROTOCOL, "pdf": _c.TC_OUT_PDF,
    "eps": _c.TC_OUT_EPS, "latex": _c.TC_OUT_LATEX,
}


def _output_opts(format="ansi", *, depth="truecolor", protocol="auto",
                 width=0, height=0, cell_width=10, cell_height=20):
    o = _c.OutputOpts()
    key = (format or "ansi").lower() if isinstance(format, str) else format
    if isinstance(key, str) and key not in _OUT_FORMAT:
        raise TermchartError("unknown output format %r" % (format,))
    if isinstance(key, str):
        if key not in _OUT_FORMAT:
            raise TermchartError("unknown output format %r" % (format,))
        o.format = _OUT_FORMAT[key]
    else:
        o.format = int(key)
    o.color_depth = _DEPTH.get(depth or "truecolor", _c.TC_DEPTH_TRUECOLOR)
    o.protocol = _proto_kind(protocol)
    o.width = int(width or 0)
    o.height = int(height or 0)
    o.cell_width = int(cell_width or 10)
    o.cell_height = int(cell_height or 20)
    return o


def term_size() -> tuple[int, int]:
    """从 stdout 查询终端尺寸 ``(cols, rows)``；非 tty 抛 TermchartError。"""
    s = _c.TermSize()
    err = ctypes.c_void_p()
    st = _c.tc_term_size_query(ctypes.byref(s), ctypes.byref(err))
    if st != 0:
        _raise("term_size", st, err)
    return s.cols, s.rows


def term_size_from_env() -> tuple[int, int]:
    """只读 COLUMNS/LINES 环境变量，缺省 80×24，永不失败。"""
    s = _c.tc_term_size_from_env()
    return s.cols, s.rows


def term_caps(force_tty=True) -> dict:
    """探测终端能力：unicode / color_depth / mouse / alt_screen / title_setting。"""
    c = _c.TermCaps()
    st = _c.tc_term_caps_detect(ctypes.byref(c), 1 if force_tty else 0)
    if st != 0:
        raise TermchartError("term_caps failed (%d)" % (st,))
    return {
        "unicode": bool(c.unicode), "color_depth": c.color_depth,
        "mouse": bool(c.mouse), "alt_screen": bool(c.alt_screen),
        "title_setting": bool(c.title_setting),
    }


def stdout_is_tty() -> bool:
    return bool(_c.tc_term_stdout_is_tty())


def stdin_is_tty() -> bool:
    return bool(_c.tc_term_stdin_is_tty())


def clipboard_set(text: str, *, primary=False) -> None:
    """OSC 52 写剪贴板（直接写 stdout）。"""
    err = ctypes.c_void_p()
    fn = _c.tc_term_clipboard_set_primary if primary else _c.tc_term_clipboard_set
    st = fn(text.encode("utf-8"), ctypes.byref(err))
    if st != 0:
        _raise("clipboard_set", st, err)


def hyperlink(uri: str, text: str, *, id=None) -> None:
    """OSC 8 超链接（直接写 stdout）。"""
    err = ctypes.c_void_p()
    st = _c.tc_term_hyperlink(uri.encode("utf-8"),
                              id.encode("utf-8") if id else None,
                              text.encode("utf-8"), ctypes.byref(err))
    if st != 0:
        _raise("hyperlink", st, err)


def hyperlink_close() -> None:
    err = ctypes.c_void_p()
    st = _c.tc_term_hyperlink_close(ctypes.byref(err))
    if st != 0:
        _raise("hyperlink_close", st, err)


def set_mouse(enable=True, *, mode=1002, encoding=1006) -> None:
    """鼠标模式开关：mode 1000/1002/1003，encoding 1005/1006/1015。"""
    err = ctypes.c_void_p()
    st = _c.tc_term_mouse(1 if enable else 0, int(mode), int(encoding), ctypes.byref(err))
    if st != 0:
        _raise("set_mouse", st, err)


class Terminal:
    """终端状态句柄：``with Terminal()`` 进入，退出时精确回滚。"""

    def __init__(self, *, raw_mode=False, alt_screen=True,
                 hide_cursor=True, mouse=False):
        o = _c.TermOpts()
        o.raw_mode = 1 if raw_mode else 0
        o.alt_screen = 1 if alt_screen else 0
        o.hide_cursor = 1 if hide_cursor else 0
        o.mouse = 1 if mouse else 0
        err = ctypes.c_void_p()
        self._h = _c.tc_term_create(ctypes.byref(o), ctypes.byref(err))
        if not self._h:
            _raise("Terminal", _c.TC_ERR_RENDER, err)

    def close(self):
        if self._h:
            _c.tc_term_free(self._h)
            self._h = None

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()


# ── 自绘画布 / 增量渲染 ─────────────────────────────────────────────────────

class Canvas:
    """字符格画布：自己落格，交给 Renderer 做逐帧差量输出。"""

    def __init__(self, width: int, height: int):
        err = ctypes.c_void_p()
        self._h = _c.tc_canvas_create(int(width), int(height), ctypes.byref(err))
        if not self._h:
            _raise("Canvas", _c.TC_ERR_INVALID_ARG, err)

    def __del__(self):
        if getattr(self, "_h", None):
            _c.tc_canvas_free(self._h)
            self._h = None

    def _alive(self):
        if not getattr(self, "_h", None):
            raise TermchartError("Canvas has been closed")
        return self._h

    @property
    def size(self) -> tuple[int, int]:
        return _c.tc_canvas_width(self._alive()), _c.tc_canvas_height(self._alive())

    def resize(self, width: int, height: int) -> None:
        if _c.tc_canvas_resize(self._alive(), int(width), int(height)) != 0:
            raise TermchartError("invalid canvas size")

    def clear(self) -> None:
        _c.tc_canvas_clear(self._alive())

    @staticmethod
    def _style(style):
        return ctypes.byref(style) if style is not None else None

    def put(self, x: int, y: int, codepoint: str, style=None) -> bool:
        cp = ord(codepoint) if isinstance(codepoint, str) else int(codepoint)
        return bool(_c.tc_canvas_put(self._alive(), int(x), int(y), cp, self._style(style)))

    def text(self, x: int, y: int, s: str, style=None) -> bool:
        return bool(_c.tc_canvas_text(self._alive(), int(x), int(y),
                                      s.encode("utf-8"), self._style(style)))

    def line(self, x0: int, y0: int, x1: int, y1: int,
             codepoint="*", style=None) -> bool:
        cp = ord(codepoint) if isinstance(codepoint, str) else int(codepoint)
        return bool(_c.tc_canvas_line(self._alive(), int(x0), int(y0), int(x1), int(y1),
                                      cp, self._style(style)))

    def fill_rect(self, x: int, y: int, w: int, h: int,
                  codepoint="█", style=None) -> None:
        cp = ord(codepoint) if isinstance(codepoint, str) else int(codepoint)
        _c.tc_canvas_fill_rect(self._alive(), int(x), int(y), int(w), int(h),
                               cp, self._style(style))

    def render(self, format="ansi", **kwargs) -> bytes:
        """按 format 导出字节流；两段式缓冲。"""
        return _two_phase_bytes(
            lambda buf, cap, need, err: _c.tc_canvas_render(
                self._alive(), ctypes.byref(_output_opts(format, **kwargs)), buf, cap, need, err),
            "canvas render",
        )

    def ansi(self, *, depth="truecolor") -> str:
        return self.render("ansi", depth=depth).decode("utf-8", "replace")


class Renderer:
    """增量帧渲染器：与上一帧比较后只输出变化部分。"""

    def __init__(self, depth="truecolor"):
        err = ctypes.c_void_p()
        self._h = _c.tc_renderer_create(_DEPTH.get(depth, _c.TC_DEPTH_TRUECOLOR),
                                        ctypes.byref(err))
        if not self._h:
            _raise("Renderer", _c.TC_ERR_RENDER, err)

    def __del__(self):
        if getattr(self, "_h", None):
            _c.tc_renderer_free(self._h)
            self._h = None

    def reset(self) -> None:
        _c.tc_renderer_reset(self._h)

    def frame(self, canvas: Canvas) -> str:
        """输出本帧差量字节串（首帧/尺寸变化自动全量）。"""
        return _two_phase_bytes(
            lambda buf, cap, need, err: _c.tc_renderer_frame(
                self._h, canvas._alive(), buf, cap, need, err),
            "renderer frame",
        ).decode("utf-8", "replace")


# ── 输入解析 / 命中区域 ─────────────────────────────────────────────────────

_KEY_NAMES = {getattr(_c, "TC_KEY_" + k.upper().replace("_", "_")): k
              for k in ("unknown", "char", "enter", "tab", "backspace", "escape",
                        "up", "down", "left", "right", "home", "end", "page_up",
                        "page_down", "insert", "delete")}

_EVENT_NAMES = {_c.TC_EVENT_KEY: "key", _c.TC_EVENT_MOUSE: "mouse",
                _c.TC_EVENT_RESIZE: "resize", _c.TC_EVENT_FOCUS: "focus",
                _c.TC_EVENT_PASTE: "paste", _c.TC_EVENT_UNKNOWN: "unknown"}


class InputParser:
    """跨 feed 保持状态（UTF-8 半包 / 转义半包）的终端输入解析器。"""

    def __init__(self):
        err = ctypes.c_void_p()
        self._h = _c.tc_input_create(ctypes.byref(err))
        if not self._h:
            _raise("InputParser", _c.TC_ERR_RENDER, err)

    def __del__(self):
        if getattr(self, "_h", None):
            _c.tc_input_free(self._h)
            self._h = None

    def reset(self) -> None:
        _c.tc_input_reset(self._h)

    def feed(self, data: bytes) -> list[dict]:
        if isinstance(data, str):
            data = data.encode("utf-8")
        count = ctypes.c_uint64(0)
        err = ctypes.c_void_p()
        st = _c.tc_input_feed(self._h, data, len(data), None, 0,
                              ctypes.byref(count), ctypes.byref(err))
        if st != 0:
            _raise("input feed", st, err)
        if count.value == 0:
            return []
        arr = (_c.Event * count.value)()
        got = ctypes.c_uint64(0)
        st = _c.tc_input_feed(self._h, data, len(data), arr, count.value,
                              ctypes.byref(got), ctypes.byref(err))
        if st != 0:
            _raise("input feed", st, err)
        out = []
        for e in arr[: got.value]:
            kind = _EVENT_NAMES.get(e.kind, "unknown")
            item = {"kind": kind, "mods": e.mods}
            if kind == "key":
                item["key"] = e.key
                if e.key == _c.TC_KEY_CHAR:
                    item["text"] = chr(e.codepoint)
            elif kind == "mouse":
                item.update(button=e.button, action=e.action, x=e.x, y=e.y)
            elif kind == "resize":
                item.update(cols=e.cols, rows=e.rows)
            elif kind == "focus":
                item["focused"] = bool(e.focused)
            elif kind == "paste":
                item["text"] = ctypes.string_at(e.text, e.text_len).decode("utf-8", "replace")
            out.append(item)
        return out


class Regions:
    """命中区域集合：payload 由调用方登记，命中时原样回报。"""

    def __init__(self):
        err = ctypes.c_void_p()
        self._h = _c.tc_regions_create(ctypes.byref(err))
        if not self._h:
            _raise("Regions", _c.TC_ERR_RENDER, err)

    def __del__(self):
        if getattr(self, "_h", None):
            _c.tc_regions_free(self._h)
            self._h = None

    def clear(self) -> None:
        _c.tc_regions_clear(self._h)

    @property
    def count(self) -> int:
        return _c.tc_regions_count(self._h)

    def add_rect(self, x, y, w, h, payload=0) -> None:
        _c.tc_regions_add_rect(self._h, int(x), int(y), int(w), int(h), int(payload))

    def add_circle(self, cx, cy, radius, payload=0) -> None:
        _c.tc_regions_add_circle(self._h, int(cx), int(cy), float(radius), int(payload))

    def add_polygon(self, points, payload=0) -> None:
        xy = (ctypes.c_int32 * (2 * len(points)))()
        for i, (x, y) in enumerate(points):
            xy[2 * i], xy[2 * i + 1] = int(x), int(y)
        err = ctypes.c_void_p()
        if not _c.tc_regions_add_polygon(self._h, xy, len(points), int(payload),
                                         ctypes.byref(err)):
            _raise("add_polygon", _c.TC_ERR_INVALID_ARG, err)

    def hit(self, x, y) -> list[int]:
        count = ctypes.c_uint64(0)
        st = _c.tc_regions_hit(self._h, float(x), float(y), None, 0, ctypes.byref(count))
        if st != 0:
            raise TermchartError("regions hit failed (%d)" % st)
        if count.value == 0:
            return []
        arr = (ctypes.c_uint32 * count.value)()
        st = _c.tc_regions_hit(self._h, float(x), float(y), arr, count.value,
                               ctypes.byref(count))
        if st != 0:
            raise TermchartError("regions hit failed (%d)" % st)
        return list(arr[: count.value])


# ── 图表：更多导出物种 / 图像句柄 ───────────────────────────────────────────

def chart_export(chart, format="pdf", *, width=0, height=0, depth="truecolor",
                 title=None, show_legend=False) -> bytes:
    """图表导出 PDF/EPS/LaTeX/JSON/TEXT/HTML_PRE 等物种。"""
    po = _plot_opts(width, height, title, show_legend, depth)
    return _two_phase_bytes(
        lambda buf, cap, need, err: _c.tc_chart_export(
            chart._alive(), ctypes.byref(_output_opts(format, depth=depth)),
            ctypes.byref(po), buf, cap, need, err),
        "chart export",
    )


def chart_to_image(chart, *, width=0, height=0, title=None,
                   show_legend=False, depth="truecolor") -> "Image":
    """图表渲染成 Image 句柄，之后可复用任意 image 编码格式。"""
    err = ctypes.c_void_p()
    po = _plot_opts(width, height, title, show_legend, depth)
    h = _c.tc_chart_to_image(chart._alive(), ctypes.byref(po), ctypes.byref(err))
    if not h:
        _raise("chart_to_image", _c.TC_ERR_RENDER, err)
    img = Image.__new__(Image)
    img._h = h
    return img


# ── 时序图 ──────────────────────────────────────────────────────────────────

class SequenceDiagram:
    """mermaid ``sequenceDiagram`` 文本 → 时序图。"""

    def __init__(self, source: str):
        err = ctypes.c_void_p()
        self._h = _c.tc_seq_from_text(source.encode("utf-8"), ctypes.byref(err))
        if not self._h:
            _raise("SequenceDiagram", _c.TC_ERR_INVALID_ARG, err)

    def __del__(self):
        if getattr(self, "_h", None):
            _c.tc_seq_free(self._h)
            self._h = None

    @property
    def counts(self) -> tuple[int, int]:
        return (_c.tc_seq_participant_count(self._h),
                _c.tc_seq_message_count(self._h))

    def render(self, format="ansi", *, width=80, height=24, **kwargs) -> bytes:
        return _two_phase_bytes(
            lambda buf, cap, need, err: _c.tc_seq_render(
                self._h, ctypes.byref(_output_opts(format, width=width, height=height, **kwargs)),
                buf, cap, need, err),
            "sequence render",
        )


def sequence_from_text(source: str) -> SequenceDiagram:
    return SequenceDiagram(source)


# ── 多帧编码 ────────────────────────────────────────────────────────────────

def encode_frames(images, format="gif", *, quality=85, fps=25) -> bytes:
    """多帧 GIF 动画 / MP4 视频。AV1 未开放，传 avif/av1 抛 TermchartError。"""
    arr = (ctypes.c_void_p * len(images))()
    for i, im in enumerate(images):
        arr[i] = im._alive()
    return _two_phase_bytes(
        lambda buf, cap, need, err: _c.tc_image_encode_frames(
            arr, len(images), format.encode("utf-8"), int(quality), int(fps),
            buf, cap, need, err),
        "encode_frames",
    )


# ── 3D / 4D ─────────────────────────────────────────────────────────────────

class Scene3D:
    """scatter3d / surface3d / wire4d 的 C 面句柄。"""

    def __init__(self, kind: str):
        err = ctypes.c_void_p()
        self._h = _c.tc_scene3d_create(kind.encode("utf-8"), ctypes.byref(err))
        if not self._h:
            _raise("Scene3D", _c.TC_ERR_INVALID_ARG, err)
        self.kind = kind

    def __del__(self):
        if getattr(self, "_h", None):
            _c.tc_scene3d_free(self._h)
            self._h = None

    @staticmethod
    def _check(st, what):
        if st != 0:
            raise TermchartError("%s failed (%d)" % (what, st))

    def set_points(self, points) -> None:
        """scatter3d: [(x,y,z), ...]；wire4d: [(x,y,z,w), ...]。"""
        flat = [float(v) for p in points for v in p]
        arr = (ctypes.c_double * len(flat))(*flat)
        err = ctypes.c_void_p()
        self._check(_c.tc_scene3d_set_points(self._h, arr, len(flat), ctypes.byref(err)),
                    "set_points")

    def set_grid(self, z, cols: int, rows: int) -> None:
        flat = [float(v) for row in z for v in row] if z and isinstance(z[0], (list, tuple)) \
            else [float(v) for v in z]
        arr = (ctypes.c_double * len(flat))(*flat)
        err = ctypes.c_void_p()
        self._check(_c.tc_scene3d_set_grid(self._h, arr, int(cols), int(rows), ctypes.byref(err)),
                    "set_grid")

    def set_edges(self, pairs) -> None:
        flat = [int(v) for p in pairs for v in p]
        arr = (ctypes.c_uint32 * len(flat))(*flat)
        err = ctypes.c_void_p()
        self._check(_c.tc_scene3d_set_edges(self._h, arr, len(flat), ctypes.byref(err)),
                    "set_edges")

    def set_view(self, azimuth=35.0, elevation=25.0, fovy=55.0) -> None:
        self._check(_c.tc_scene3d_set_view(self._h, float(azimuth), float(elevation),
                                           float(fovy)), "set_view")

    def set_range(self, axis: int, lo: float, hi: float) -> None:
        err = ctypes.c_void_p()
        self._check(_c.tc_scene3d_set_range(self._h, int(axis), float(lo), float(hi),
                                            ctypes.byref(err)), "set_range")

    def set_rotation4d(self, xw_deg=20.0, yz_deg=15.0) -> None:
        self._check(_c.tc_scene3d_set_rotation4d(self._h, float(xw_deg), float(yz_deg)),
                    "set_rotation4d")

    def render(self, format="ansi", *, width=72, height=28, **kwargs) -> bytes:
        return _two_phase_bytes(
            lambda buf, cap, need, err: _c.tc_scene3d_render(
                self._h, ctypes.byref(_output_opts(format, width=width, height=height, **kwargs)),
                buf, cap, need, err),
            "scene3d render",
        )
