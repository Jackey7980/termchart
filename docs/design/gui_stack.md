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

widget/plugin/app 三个模块目录在 `include/termchart/` 下**尚不存在**（结构.md 规划、
未落地）；GUI 平行栈本身是规格设想，公开头里没有 GUI 专属 API。
