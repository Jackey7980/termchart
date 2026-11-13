# GUI 栈定位：复用边界与重写边界

## 决策

GUI（窗口/位图渲染）与终端（字符格渲染）是**平行栈**，共享的只有语义层：

- **复用边界是 `scene`（L4）及以下**：L0–L5 的语义与数据类型两边共用——
  `Scene`/`Style`/`core::Rect`、`color::resolve`、`image::Bitmap` 的合成原语、
  `image/convert`、`raster` 注册表（把标量场压成子像素的思想 GUI 同样能用）；
- **`layout / widget / interaction / app` 四层由 GUI 自己重写**：
  终端的布局常数（1:2 字符格比）、焦点模型（tty 键盘）、widget 树
  对位图渲染器都不适用。

## 不要带进 GUI 的东西

- `cell` / SGR / `CellBuffer`：终端字符格是"低分辨率 + 颜色对"，
  GUI 的原子是**像素矩形 + 真彩**；
- 需要位图就走 `color::resolve` + `image/convert`：注意 `resolve_*` 会**先按
  `ColorDepth` 量化再取色**——GUI 显示的颜色必须与终端实际显示一致，
  不能让 GUI 假装能显示真彩。

## 共享面清单

| 能力 | 位置 | 两边共用方式 |
| --- | --- | --- |
| 语义场景 | `scene/command.hpp` | GUI 直接消费同一 Scene 指令集 |
| 颜色解析 | `color/resolve.hpp` | 按 GUI 目标深度量化 |
| 位图 | `image/bitmap.hpp` | GUI 的输出就是 Bitmap |
| 标量场 | `image/field.hpp` | 热力/密度类可视化共用 |
| 数据 | `chart/dataset.hpp` | Dataset 不认识画布，GUI 可复用 |

## 为什么不共享更多

把渲染栈强行统一（比如让 GUI 也走 CellBuffer）需要把所有像素决策降级成字符决策，
两边都会变差；把语义层复制一份则会导致同一张图在终端和 GUI 里语义分叉。
取中间：语义层一份，呈现层两份。

## 现状

widget/plugin/app 已在 `include/termchart/` 落地（终端栈使用）。GUI 平行栈的**渲染核心**已落地：
`include/termchart/gui/`（层 22）提供 `font`（`FontBackend` 多态：ASCII 内置 5×7 点阵 + 非 ASCII 惰性加载系统 TTF）与 `scene_renderer`（Scene → RGBA 位图，
超采样抗锯齿），复用 `scene::StatefulVisitor` 时序与 `image/png_encode` 出图，可运行产物见
`examples/55_gui_scene_png.cpp`。

**平行栈本体**（`include/termchart_gui/`，独立顶层 header-only 库）已落地：`backend_tk`
（抽象 + 无头/终端两种零依赖参考后端）、`layout`（约束/弹性/网格）、`widget_tree`
（按钮/复选/滑块/进度条/标签/容器 + ChartWidget/CandlestickWidget 等组合图控件）、
`native_loop`（事件循环抽象 + 定时器）、`render/style_map`。控件/布局/绘制只面向
`TkCanvas` 抽象，换后端零改动。

**真实后端矩阵**（均为可选插件：由顶层按平台自动纳入，缺依赖时插件自行跳过并打印
`STATUS`，不拖累核心零依赖构建；显式 `-DTERMCHART_BUILD_GUI_QT/TK/WIN32=ON` 才编 Windows/Qt/Tk）：

| 后端 | 位置 | 依赖 | 状态 |
| --- | --- | --- | --- |
| Win32 | `plugins/gui_win32` | 系统 user32/gdi32 | 已编译验证（`--smoke` 实证） |
| Qt | `plugins/gui_qt` | Qt5/Qt6 Widgets | 已真编译 + 冒烟（Qt 6.8.3 动态链接，2026-11-07） |
| Tk | `plugins/gui_tk` | Tcl/Tk 开发库 | 代码已写，本机无 Tcl/Tk 未编译验证 |
| X11 | `plugins/gui_x11` | libX11 | 已编译 + Xvfb 无头冒烟（`x11_smoke` ctest，2026-11-11） |
| Cocoa | `plugins/gui_cocoa` | AppKit / CoreGraphics（系统框架） | 代码已写，待 macOS CI 编译验证（`cocoa_offscreen`） |
| JNI | `plugins/gui_jni` | JDK（`jni.h`） | 已编译 + 真 JVM 冒烟（`jni_smoke`，2026-11-11） |

X11 / Cocoa / JNI 走 **Take 2「句柄注入」**：窗口与视图都归宿主所有，后端只把
`BitmapCanvas` 贴到宿主表面、把宿主剥壳后的原生事件排队回 `inject_event`；平台类型
（`<X11/Xlib.h>` / `<AppKit/AppKit.h>` / `jni.h`）只出现在插件 `src/*.cpp|.mm`，公开头
里只有 `NativeSurface`（`void*` + 尺寸）。许可上 Xlib 与系统框架均宽松/内置；JNI 仅在
编译期用宿主 JDK 的 `jni.h`，运行期 JRE 由使用者提供，**不 vendoring** 任何 JDK 二进制。

接真实工具包 = 外部依赖 + 编译型平台代码，违背核心零强制依赖硬约束，故一律归为
可选插件不进核心；`widget_tree/layout/native_loop` 对全部后端不变。
