"""termchart_capi —— 基于 C ABI 的 Python 绑定（ctypes）。

对应 C 侧 termchart_capi 共享库（见 libs/capi）。_capi 模块由
tools/gen_python_capi.py 从 termchart_capi.h 生成（加载库 + ctypes 原型）；
本模块再裹一层 Pythonic API，能力面与 C++ 便捷入口 api:: 对齐：

    文本    render_line / print_line / hello（可带颜色与属性）
    图表    plot / scatter / bar（返回 ANSI 字符串） + print_chart（直接写 stdout）
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
    "plot", "scatter", "bar", "print_chart", "Chart",
    "render_line", "print_line", "hello",
    "write_stdout", "write_stderr", "flush",
    "auto_plot", "version", "TermchartError",
]

_c._load()  # 加载共享库（自动搜索常见构建目录 / TERMCHART_CAPI_LIB）

_DEPTH = {
    "none": _c.TC_DEPTH_NONE,
    "16": _c.TC_DEPTH_16,
    "256": _c.TC_DEPTH_256,
    "truecolor": _c.TC_DEPTH_TRUECOLOR,
}

_ATTR = {"bold": 1, "dim": 2, "italic": 4, "underline": 8, "reverse": 16}


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

def _render(kind: str, **data) -> str:
    factory = {
        "line": _c.tc_chart_create_line,
        "scatter": _c.tc_chart_create_scatter,
        "bar": _c.tc_chart_create_bar,
    }[kind]
    chart = factory()
    if not chart:
        raise TermchartError("failed to create chart handle (out of memory?)")
    try:
        if kind == "bar":
            values = data["values"]
            n = len(values)
            vals = (ctypes.c_double * n)(*values)
            labels = data.get("labels")
            if labels:
                arr = (ctypes.c_char_p * len(labels))(
                    *[l.encode("utf-8") for l in labels]
                )
                labels_ptr = ctypes.cast(arr, ctypes.POINTER(ctypes.c_char_p))
                n_labels = len(labels)
            else:
                labels_ptr = None
                n_labels = 0
            st = _c.tc_chart_add_bar(chart, vals, n, labels_ptr, n_labels)
        else:
            x = data["x"]
            y = data["y"]
            n = len(x)
            xs = (ctypes.c_double * n)(*x)
            ys = (ctypes.c_double * n)(*y)
            st = _c.tc_chart_add_points(chart, xs, ys, n)
        if st != 0:
            raise TermchartError("tc_chart_add_* failed: status %d" % st)

        opts = _c.PlotOpts()
        opts.width = data.get("width", 0)
        opts.height = data.get("height", 0)
        title = data.get("title")
        opts.title = title.encode("utf-8") if title else None
        opts.depth = _DEPTH.get(data.get("depth", "truecolor"), _c.TC_DEPTH_TRUECOLOR)
        opts.show_legend = 1 if data.get("show_legend") else 0

        if data.get("to_stdout"):
            st = _c.tc_chart_print(chart, ctypes.byref(opts), None)
            if st != 0:
                raise TermchartError("tc_chart_print failed: status %d" % st)
            return ""

        out = ctypes.c_char_p()
        err = ctypes.c_void_p()
        st = _c.tc_chart_render_ansi(
            chart, ctypes.byref(opts), ctypes.byref(out), ctypes.byref(err)
        )
        if st != 0:
            _raise("render", st, err)

        result = out.value
        _c.tc_free_string(out)
        return result.decode("utf-8") if isinstance(result, bytes) else result
    finally:
        _c.tc_chart_free(chart)


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
    """读 CSV/JSONL → AutoViz 选图 → 出图到 stdout。"""
    err = ctypes.c_void_p()
    rc = _c.tc_auto_plot(str(path).encode("utf-8"), ctypes.byref(err))
    if rc != 0:
        _raise("auto_plot", rc, err)


# ── 句柄式图表（对齐 tc_chart_* 与 C++ chart::Chart 能力面）──────────────────────

_PROP = {
    "y_domain": _c.TC_PROP_Y_DOMAIN,
    "auto_domain": _c.TC_PROP_AUTO_DOMAIN,
    "include_zero": _c.TC_PROP_INCLUDE_ZERO,
    "emphasize_zero": _c.TC_PROP_EMPHASIZE_ZERO,
    "shade_negative": _c.TC_PROP_SHADE_NEGATIVE,
    "x_ticks": _c.TC_PROP_X_TICKS,
}
_BOOL_PROPS = {"auto_domain", "include_zero", "emphasize_zero", "shade_negative"}


class Chart:
    """句柄式图表：多系列 + 模型属性 + ANSI / SVG 输出。

    与 C 侧 ``tc_chart_*`` 一一对应；同一对象可反复渲染（ANSIS 缓存只构建一次图）。

        with Chart("line") as c:
            c.add_series([0,1,2,3], [1,4,9,16], name="sq", color=(255,140,0))
            c.add_series([0,1,2,3], [2,3,5,7], name="lin", color=6)  # 6 = ANSI16 cyan
            c.set_prop("y_domain", (0, 18))
            print(c.ansi(width=60, height=16, title="demo", show_legend=True))
            open("out.svg", "w").write(c.to_svg(width=60, height=16))
    """

    _FACTORY = {
        "line": _c.tc_chart_create_line,
        "scatter": _c.tc_chart_create_scatter,
        "bar": _c.tc_chart_create_bar,
    }

    def __init__(self, kind="line"):
        if kind not in self._FACTORY:
            raise TermchartError("unknown chart kind: %r" % (kind,))
        self._h = self._FACTORY[kind]()
        if not self._h:
            raise TermchartError("failed to create chart handle")
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

    # ── 输出 ──
    def _opts(self, width, height, title, show_legend, depth):
        o = _c.PlotOpts()
        o.width = int(width)
        o.height = int(height)
        o.title = title.encode("utf-8") if title else None
        o.depth = _DEPTH.get(depth, _c.TC_DEPTH_TRUECOLOR)
        o.show_legend = 1 if show_legend else 0
        return o

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
