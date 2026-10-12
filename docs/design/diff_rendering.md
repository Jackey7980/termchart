# 差分重绘

## 决策

终端实时路径只在**变化的部分**输出字节：

- `render::Renderer` 持有 prev `CellBuffer` 状态与 `DamageTracker`；
- `render::diff.hpp` 是纯函数层：输入两个 `CellBuffer`，输出 `DiffRegion{row, begin, end}`
  （变化的连续段）；
- 合并规则：同一行内相邻/重叠的变化段合并（`merge_gap` 默认 4）——多写几格内容比
  多发一次光标定位 + 样式序列便宜；
- 尺寸不同视为"全变"，`identical()` 返回 true 时调用方必须走全量重绘；
- `Renderer::render()` 首帧或尺寸变化自动退化为全量；`render_full()` 显式全量；
  `reset()` 丢弃 prev（终端被清屏后必须调用，否则 diff 基线失效）。

## 为什么有价值

图表典型帧间变化是局部（新增数据点、移动光标、tooltip），全量重发等于每帧重画全屏；
差分把输出量压到与变化面积成正比。SGR 侧配合 `render/sgr.hpp` 的
`append_style_delta()`（只发变化的那部分样式），而 `style_sequence()` 是完整序列
（未设的前/背景会发 39/49）——帧内路径一律走 delta。

## 边界

- `Renderer` 只生成字节、不写 stdout——CI/headless/单测完全可用；写动作收在
  `api/print.hpp`（唯一触碰进程全局状态的地方）。
- snapshot 导出（backend/snapshot/*）刻意**无 diff、无 prev**：一次性导出没有
  "上一帧"概念，两者是物种区别不是实现疏漏。

## 验证

- golden 快照（tests/golden/ansi_props/*）把渲染字节流解析回属性再比对，锁定
  行/列/码点/前景/背景/属性位的正确性；
- `bench/bench_render`（规格）对应差分重绘吞吐基线。
