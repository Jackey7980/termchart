# 架构总览

## 分层

模块 = `include/termchart/` 下的一级目录，赋予层号 L0–L22（机器强制，规则唯一真源在
`cmake/TermchartModules.cmake`，由 `tools/check_layers.py` 执行）。同层模块互不依赖：

```
core(0) util(1) math(1) color(2) io(2) scale(3) math3d(3) math4d(3) scene(4)
data(4) image(5) raster(6) term(7) network(7) document(8) protocol(9) canvas(10)
layout(11) chart(12) chart3d(12) chart4d(12) graph(13) render(14) interaction(15)
backend(16) dsl(17) widget(18) plugin(19) app(20) api(21) gui(22)
```

规格外扩展（10-19 起）：`math3d`/`math4d`/`chart3d`/`chart4d`（高维图表）、
`network`（TCP/HTTP/TLS）、`math`/`io`/`data`（数据管线，见
[guide/data_pipeline.md](guide/data_pipeline.md)）。

- 层号低 → 只能被高层依赖；`api`（L21）是唯一能碰所有模块的顶层。
- 例外：`detail/` 层号 -1（任何层可 include）；顶层 `version/export/deprecate/fwd` 视为 core。
- 关键禁止边：`backend > chart`、`chart > render`、`render > interaction`、`gui > canvas/term/app`
  （细节见 [design/layering.md](design/layering.md) 与 [design/modularity.md](design/modularity.md)）。

## 中枢与三条渲染路径

`core::CellBuffer`（字符格网格）是全库中枢。三条下游路径在数据表示处分野：

| 路径 | 数据表示 | 走向 |
| --- | --- | --- |
| 终端实时 | `CellBuffer` | → `render::Renderer`（持 prev + diff）→ 增量字节流 → 调用方写 stdout |
| snapshot 导出 | `CellBuffer` | → `backend/snapshot/*`（ansi / text / html_pre / json），一次性无 diff |
| vector 导出 | `scene::Scene` | → `backend/vector/*`（svg；规格还含 TikZ） |
| structured 导出 | `document::Document` | → `backend/structured/*`（html / markdown） |

矢量 IR 的上游：`Scene`（非完备图形 IR，scene/command.hpp 定义指令集）由图表经 `Chart::to_scene`
产出（`draw(DrawContext)` 仍作为 Canvas 直绘回退并存），再由 `canvas/scene_renderer` 收口进 `Canvas`，
再 `to_buffer()` 进 `CellBuffer`。

## 关键不变量

- `render/sgr.hpp` 是唯一 SGR 序列生成点（实时路径与 snapshot 后端共用）。
- 宽字符占两格，生产者负责给右半格填 `kContinuationCp`；渲染器负责跳过它。
- 图表字符化不依赖颜色可辨：饼图等给每个系列/扇区专属填充字符（█▓▒░■●▲◆○ 循环），
  单色终端仍可读。
- 错误处理：不抛异常；`core/expected.hpp` 提供 `Expected<T,E>`（语义对齐 `std::expected`）、
  `Result<T>` 与 `Status`。只回报成败返回 `Status`，需带值用 `Result<T>`。
- 平台隔离：公开头无 `#ifdef`，平台分支全在 `src/term/*_posix.cpp` / `*_win32.cpp`。

## 图可视化子树

`graph/`：语义层 `Graph`（节点/边）→ 布局（`graph/layout`：分层/力导向/树/环形/径向/地铁）→
路由（`graph/route`：正交/直线/弧线/样条/障碍绕行/聚束）→ 形状（`graph/shape`）→
端到端图（`graph/diagrams`：flowchart / state_machine / mindmap / orgchart / sankey /
uml_* / er_diagram / metro_map / petri_net / circuit / chord / arc / adjacency_matrix /
social_network / knowledge_graph / dependency / dag / syntax_tree）。

## GUI 复用边界

GUI 栈只复用 L0–L4（`scene` 及以下，含 `scene`/`style`/`dataset`/`color` 等语义类型）；`layout`/`widget`/`interaction`/`app` 由 GUI 自行重写（`include/termchart_gui/`）。
语义类型（`Scene`/`Style`/`Dataset`/`color::resolve`）是两边共享面，字符格与 SGR 不进 GUI。
详见 [design/gui_stack.md](design/gui_stack.md)。

## 文档与真值

- 进度：`ROADMAP.md`（唯一口径）；能力差距：`docs/reference/capability_matrix.md`（手写）。
- 生成物（勿手改）：`docs/reference/module_matrix.md`、`结构.md` 附一/附四、
  `include/termchart/detail/*.hpp`、`single_include/`、`tests/golden/`。
