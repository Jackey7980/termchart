# Python 桥（pybind11）

## 在任意目录的 .py 里使用

装一次到 site-packages，之后不需要 `PYTHONPATH`，也不用把 `.pyd` 拷来拷去：

```sh
pip install ./bindings/python
```

Windows 上 CMake 默认挑 Visual Studio，而本项目在 MinGW 下开发，用 MSVC 编会失败；
显式指定 Ninja（编译器取 PATH 里的 `g++`）：

```powershell
$env:CMAKE_GENERATOR = "Ninja"
pip install ./bindings/python
```

验证：

```sh
python -c "import termchart as tc; print(tc.version(), tc.__file__)"
```

之后在任何目录、任何终端的脚本里 `import termchart` 都指向 site-packages 里的
`termchart.cpXXX-*.pyd`。改完 C++ 源码后重跑同一条 `pip install` 即可升级。

> 免编译的 ctypes 版 `termchart_capi` 不走 pip：把 `bindings/python` 加进
> `PYTHONPATH`，并设 `TERMCHART_CAPI_LIB` 指向 `libtermchart_capi.dll`
> （在 `build/<preset>/libs/capi/`），详见 [C ABI 与 Python 桥](c_api.md)。

Windows 控制台默认按 GBK 解码，而图表含 `•` `█` 等字符，`print` 会抛
`UnicodeEncodeError`。终端里临时开 UTF-8 模式：

```powershell
$env:PYTHONUTF8 = "1"
```

永久生效用 `setx PYTHONUTF8 1`（新开终端起效），或改用 Windows Terminal / VS Code 终端。
`save_png` / `save_svg` 走 UTF-8 路径，中文用户名目录可以直接用。

## 从源码手工构建

默认不构建（`TERMCHART_BUILD_PYTHON=OFF`）。开启并构建：

```sh
cmake -S . -B build/py -G Ninja -DTERMCHART_BUILD_PYTHON=ON
cmake --build build/py --target termchart_pybind
# 产物在 build/py/bindings/python/termchart.*.pyd/.so
```

零配置起步（无需 numpy / pandas）：

```python
import termchart as tc
print(tc.plot([0, 1, 2, 3], [1, 4, 2, 8]))
tc.save_png([0, 1, 2, 3], [1, 4, 2, 8], "out.png")
```

> **两套 Python 绑定**：上面是基于 pybind11、直接吃 C++ 头的 `termchart`（高层 `plot`/
> `save_png`/`to_rgba` 等）。另有基于 **C ABI** 的 `termchart_capi` 包（纯 ctypes，无需编译
> 扩展，且与 Rust/Java 桥共用同一 C 前置），提供句柄式 `Chart` / `set_prop` / `ansi` /
> `to_svg` / `to_png`，并能在不碰 C++ 核心的前提下加坐标轴标签。新项目与跨语言桥对齐优先
> 选它，详见 [C ABI 与 Python 桥](c_api.md)。

## numpy 零拷贝

`x / y / values` 接受 `numpy.ndarray`（任意数值 dtype：f32/f64/int/uint），
连续数组直接读缓冲，不落中间 `std::vector`：

```python
import numpy as np
x = np.linspace(0, 6.28, 256, dtype=np.float32)
y = np.sin(x)
print(tc.scatter(x, y))
```

## pandas 便捷接口

DataFrame / Series 通过 `.to_numpy()` 取列（运行时 duck-typed，构建期零依赖）：

```python
import pandas as pd
df = pd.DataFrame({"t": np.arange(100), "v": np.random.randn(100)})
print(tc.plot_df(df, "t", "v"))          # 两列
print(tc.plot_df(df, y="v"))             # 省略 x -> 用索引
print(tc.plot_series(df["v"]))           # 单列 Series
```

## 像素缓冲（Tkinter / PIL / 自定义嵌图）

不依赖任何 GUI 后端，把图表渲染成 RGB(A) 像素，自行贴到任意表面：

```python
rgba = tc.to_rgba(x, y, pixel_width=960, pixel_height=540)   # np.ndarray (H,W,4)
png_bytes = tc.to_png_bytes(x, y)                            # bytes
# 无 numpy 也能拿字节：
rgb = tc.to_rgb_bytes(x, y, pixel_width=640, pixel_height=360)
# 反向：把自己编辑过的像素缓冲编码成 PNG
tc.save_png_pixels(rgb, 640, 360, "from_buffer.png")
```

完整 Tkinter 示例见 `examples/79_tkinter_demo.py`（需 Pillow）。

## Qt 窗口（需 Qt 构建）

`show_qt` 仅在 `TERMCHART_BUILD_GUI_QT=ON` 的构建里存在（默认 wheel 不含 Qt，LGPL
动态链接由使用者自备）。示例见 `examples/80_qt_demo.py`。

## 性能

光栅类函数（像素输出 / PNG）走 C++ 的 `SceneRenderer` 一次性出图，Python 侧零拷贝
搬运缓冲；numpy 数组直接读底层 buffer，不在 Python 层做逐元素循环。
