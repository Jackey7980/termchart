# Python 桥（pybind11）

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
