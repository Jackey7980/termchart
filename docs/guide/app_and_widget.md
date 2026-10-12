# app 与 widget 组装

## 现状

`app/` 与 `widget/` 两个模块目录在 `include/termchart/` 下**尚不存在**
（结构.md 规划的 L18/L20）。本篇说明规格意图与"今天能组到什么程度"。

## 规格意图（结构.md）

- `widget/`（L18）：可组合的界面控件（图表控件、图例控件、输入框…），
  建立在 layout 与 interaction 之上；
- `app/`（L20）：应用骨架——主循环、widget 树、事件路由、焦点管理、
  帧调度；`api/`（L21）是它的门面。

## 今天能组到什么程度（已落地的积木）

| 积木 | 头文件 |
| --- | --- |
| 主循环原料：终端 RAII + 字节读写 | term/terminal.hpp、api/print.hpp |
| 事件泵：解析 + 分发 + 焦点 | interaction/input_parser.hpp、dispatcher.hpp、focus.hpp |
| 布局：绘图区/网格/边距/分面 | layout/box.hpp、grid.hpp、facet.hpp、auto_margin.hpp |
| 交互控件：tooltip/缩放平移/选择 | interaction/tooltip、zoom、pan、select |
| 命中测试 | chart/hit_region.hpp + interaction/hit_test.hpp |
| 差分重绘 | render/renderer.hpp |

手动组装一个"仪表盘"主循环的骨架：

```cpp
termchart::term::Terminal term({.raw_mode = true, .alt_screen = true,
                                .hide_cursor = true, .enable_mouse = true});
term.enter();
termchart::interaction::Dispatcher dispatch;
// 订阅 Key/Mouse/Resize → 更新状态 → 重画到 CellBuffer → renderer.render()
```

参考成品：examples/06_dashboard（多面板组合）、21_hover_tooltip、
22_zoom_pan、30_legend_toggle。

## 与 GUI 栈的关系

规格里 GUI 平行栈自建 layout/widget/interaction/app（复用边界到 L4，
见 design/gui_stack.md）。终端侧 widget/app 落地时同样不应复用
`render/` 以外的呈现层代码。

## 未做

- widget 树、布局协商、焦点遍历的控件库；
- `App` 类与帧调度器；
- 声明式界面描述。
