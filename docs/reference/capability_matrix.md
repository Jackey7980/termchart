# 能力矩阵（手写，非生成物）

本文件回答三个问题：**现在真正能做什么**、**哪些路径仍是占位**、**规格里还差什么**。
它由人手维护；`docs/reference/module_matrix.md` 是生成物（`tools/gen_module_matrix.py`），
两者不要互相抄。

- 规格（做成什么样）：`结构.md`
- 计划（什么时候做）：`ROADMAP.md`
- 本文件（当前真实能力与缺口）：这里

数字口径：`include/termchart/**/*.hpp`（160 个）、`src/**/*.cpp`（124 个），
`ctest --preset ninja`（25 项 = 套件 + 17 example + 2 bench 冒烟），
doctest 402 用例 / 186806 断言，golden 9 份。改动后请重跑下列命令再更新本文件。

```bash
cmake --build --preset ninja
build/ninja/tests/termchart_tests.exe          # 单元 + 集成 + golden
ctest --preset ninja                            # 套件 + 示例 + bench 冒烟
cmake --build --preset ninja --target check_all # 分层 + 自洽 + 生成物 + 每头独立编译
```

---

## 一、模块能力（L0–L21）

| 模块 | 层 | 状态 | 真实能力 | 占位 / 缺口 |
| --- | --- | --- | --- | --- |
| `core` | 0 | 可用 | 单元格/缓冲/样式/属性/点矩形/错误码 | — |
| `util` | 1 | 可用 | UTF-8、grapheme、宽字符、文本、格式化、日志、**几何**（点线距离/严格线段相交/多边形/凸包）、**计时**（单调时钟/秒表/帧计时/超时）、**调度**（单线程定时任务，时间源可注入）、**可取消任务句柄**、**base64** | — （`base64` 是规格清单外的补充，理由见 ROADMAP 的 `util/` 条目） |
| `color` | 2 | 可用 | xterm-256/16 色表、量化、调色板、渐变、**`resolve`（Color/Style → 真正 RGB，GUI 与位图输出的分界点）** | — |
| `detail` | 2 | 生成物 | 盲文表、块字符、Unicode 属性表 | 由 `tools/` 生成，**禁止手改** |
| `term` | 7 | 可用 | ANSI 序列、能力探测、光标、平台拆分、**鼠标开关**（模式/编码两组 DECSET）、**OSC 52 剪贴板**、**OSC 8 超链接**、**DA1/DA2 主动探测与回复解析**、**颜色降级桥接** | — （规格 13 个文件全部落地） |
| `render` | 6 | 可用 | 全量/增量渲染、差分、SGR 序列化（唯一生成点）、脏区 | `compositor` 未实现 |
| `api` | 21 | 可用 | `print`、`quick`（一行式）、状态码 | 规格的 `quick_line` `quick_bar` `quick_spark` `quick_table` 未实现 |
| `scene` | 2 | 可用 | 矢量 IR（命令变体 + 访问者 + SceneBuilder）、**`bounds`（几何包围盒，GUI 决定画布/无效区时需要）** | — |
| `canvas` | 10 | 可用 | 画布、图元、`Scene → Canvas` 桥接、**扫描线填充**（横向精确覆盖 + 纵向超采样）、**路径**（曲线自适应细分、四段贝塞尔圆）、**图层合成**（Overlay/Replace/Tint + 最小重绘包围盒）、**2D 仿射变换**（含字符宽高比补偿）、**裁剪**（矩形精确 / 凸多边形 / 非凸明确不支持多边形裁剪） | `ImageOp` 仍是阴影块占位（Scene 无像素源） |
| `scale` | 2 | 可用 | 线性/对数/nice/刻度/自动域 + **注册表** | 规格的 `time` `category` `band` 未实现 |
| `image` | 5 | 部分 | `Bitmap`（RGBA，含 `fill_rect`/`blit_over`/`scaled_to`/通道转换等合成原语）、`FloatField`（标量场）、**`convert`**（位图 ↔ 标量场：Rec.601 亮度、阈值双色）、**`encode_png`**（自包含 PNG 编码：CRC-32 / Adler-32 / stored deflate） | `decoder` `png`（**解码**方向）未实现 |
| `raster` | 6 | 可用 | 盲文/半块/象限/竖条/密度字符 + 注册表 + 自动选择 | `sextant`（U+1FBxx 新区块，终端支持度低）显式推迟 |
| `layout` | 11 | 部分 | 轴/网格/图例/标题/标签、`PlotArea` | `annotation` `facet` `auto_margin` `label_avoid` 未实现 |
| `chart` | 12 | 部分 | 基类 + 系列 + 注册表 + 折线/柱状 | `context` `dataset` 未实现；规格 `cartesian/` 的 23 个图种只做了 2 个（且文件名是 `line_chart`/`bar_chart`，与规格 `line`/`bar` 不同名） |
| `document` | 8 | 可用 | Document/Block/Span/TableModel IR | — |
| `graph` | 13 | 部分 | 节点/边/元数据/占用栅格/箭头/端口/标签避让/交叉计数（判定复用 `util/geom`）；布局（网格/分层/根树/力导向）；路由（直线/正交）；**形状八种**（矩形/圆角/菱形/椭圆/胶囊/六边形/圆柱/图标）；图种（流程图/状态机/思维导图/节点-连线） | `sugiyama`（分层已覆盖其分层部分）`radial` `kamada_kawai` `circular`；`spline` `arc` `obstacle` 路由；`orgchart` `dag` `dependency` 图种 |
| `interaction` | 15 | 部分 | 输入解析（ANSI/UTF-8/SGR 鼠标流式）、事件、命中测试、分发、焦点 | `tooltip` `crosshair` `highlight` `zoom` `pan` `select` `brush` `legend_toggle` `immediate` 未实现 |
| `backend` | 16 | 可用 | 三物种各至少一个后端：snapshot（ansi/text/html_pre/json）、vector（svg）、structured（html/markdown） | `png_screenshot`；structured/vector 的 `latex_backend` |
| `protocol` | 9 | 可用 | sixel（量化+调色板+RLE）、kitty（原始 RGBA+base64 分块）、**iterm2 内联（PNG+base64+OSC 1337）**、半块图像、降级选择（Kitty > Iterm2 > Sixel > HalfBlock > None）、环境探测（含 WezTerm/Konsole/iTerm 与手动覆盖） | — |
| `dsl` | 17 | **空** | — | 零代码：`lexer` `ast` `mermaid*` `graphviz_dot` `plantuml` `d2` `table_markdown` `csv_sniff` `dsl_registry` |
| `widget` | 18 | **空** | — | 零代码 |
| `plugin` | 19 | **空** | — | 零代码 |
| `app` | 20 | **空** | — | 零代码 |
| `termchart_gui` | — | **空** | — | 平行 GUI 栈整体未开工 |

## 二、三物种导出能力

| 物种 | 输入 | 后端 | 输出 |
| --- | --- | --- | --- |
| snapshot | `CellBuffer` | `ansi` / `text` / `html_pre` / `json` | 字节流（ANSI 含 SGR，复用 `render/sgr` 唯一生成点） |
| vector | `scene::Scene` | `svg` | `<svg>`（路径/圆弧/文本/图片占位） |
| structured | `document::Document` | `html` / `markdown` | 完整 HTML 文档 / Markdown |

> 后端不做跨渲染模型转换：输入是哪一种物种，就在类型上钉死（见各 `backend/*/backend.hpp`）。

## 三、示例索引（17 个，全部在 ctest 里冒烟）

| 示例 | 路径 | 渲染方式 | 备注 |
| --- | --- | --- | --- |
| `01_hello_color` | 终端 | `render` + SGR | 颜色握手 |
| `02_sine_wave` | 终端 | 直连 `raster`（盲文/密度） | 子像素正弦波 |
| `03_function_plot` / `03_line_chart` | 终端 | `canvas` + 显式字符 + `raster` 直连 | 带坐标轴的折线 |
| `04_bar_chart` / `04_heatmap` | 终端 | 同上 | 柱状 / 热力 |
| `05_showcase` | 终端 | 直连 `raster`（盲文 + 半块双面板） | 真彩旗舰展示 |
| `06_dashboard` | 终端 | 直连 `canvas` + `raster` | 仪表盘拼版 |
| `14_flowchart` | 终端 | `graph/diagrams` → `canvas` | 分层布局 + 正交路由 |
| `15_state_machine` | 终端 | 同上 + 转移标签 | 起止标记 |
| `16_mindmap` | 终端 | 树布局 + 扇形连线 | 分支两侧分列 |
| `20_force_graph` | 终端 | 力导向 + 节点-连线视图 | 性能见 `bench_force` |
| `32_quick_one_liner` | 终端 | `api/quick` | 一行式 API |
| `34_export_svg` | 文件 | `scene::Scene` → `vector/svg` | 同一份 Scene 亦可用 `render_scene` 出终端图 |
| `35_export_html` | 文件 | `document` → `structured/html` | |
| `36_export_json` | 文件 | `CellBuffer` → `snapshot/json` | 逐格 cp/颜色/属性 |
| `38_image_sixel` | 终端 | `protocol`（kitty / iterm2 / sixel / 半块自动降级） | 本机无图像协议时输出提示；`TERMCHART_ITERM2=1` 可强制走 iTerm2 内联 |

**重要澄清（避免再踩 R12 的坑）**：`chart`/`graph`/`layout` 这些"高级 API"示例
**本来就直连 `canvas.draw_line`/`draw_text` + 显式字符**，从不经过 `scene::Scene`，
因此历史上从未出现 `*` 占位（实测占位计数为 0）。R12 的真实影响面是
**同一份 `scene::Scene` 在终端侧能否出真图**——该缝已收口：`render_scene` 的描边
走 braille 2×4 子像素、填充走 halfblock 1×2 实心（golden `scene_raster/basic.txt` 锁外观）。

## 四、性能基线（`bench/`，零外部依赖）

```bash
build/ninja/bench/bench_sugiyama          # 分层布局扩展性
build/ninja/bench/bench_force             # 力导向收敛
build/ninja/bench/bench_<name> --quick    # ctest 冒烟档
```

| bench | 规模 | 中位耗时 | 结论 |
| --- | --- | --- | --- |
| `layout_layered` | 64 节点 / 105 边 | 120 µs | 64→1024 节点耗时 ×18 ≈ 线性，符合 O(V+E) |
| `layout_layered` | 1024 节点 / 1953 边 | 2210 µs | 无平方级行为 |
| `layout_force` | 20 节点 / 200 迭代 | 2.6 ms | 20→160 节点耗时 ×56 ≈ 平方，符合每轮 O(V²) 斥力 |
| `layout_force` | 160 节点 / 200 迭代 | 146 ms | **已知性能边界**：大图需 Barnes-Hut 近似或降低迭代数 |

## 五、规格差距总览（由代码清点得出，非估计）

- 按 `结构.md` 的具名文件表核对：`include/` 现有 160 头，**规格具名头文件尚缺约 178 个**；
  `dsl/ widget/ plugin/ app/ termchart_gui/` 五个目录整体不存在。
- `examples/`：规格 60 个，实存 13 个（另有 4 个规格外示例：`03_function_plot`、`04_heatmap`、`05_showcase`、`06_dashboard`）。
- `bench/`、`tests/fuzz/`、`tests/fixtures/` 三个目录在这一轮之前**完全不存在**；`bench/` 已建（2 条基线），
  fuzz 与 fixtures 仍缺。
- `TODO` / `FIXME` 在 `include/`、`src/`、`examples/`、`tests/` 内为 **0**；占位类关键词命中 24 处，
  其中**真实缺口 3 处**（`image/decoder|png` 的**解码**方向、`scene_renderer` 的 `ImageOp`（Scene 里没有像素源）、
  `layout/internal.hpp` 的占位线格字符），其余为文档化阶段占位或正常渲染逻辑。
  `protocol/iterm2_inline` 与 `image/png_encode` 曾在此清单里，现已实现并各自带独立校验（见第三节与 `bench` 之外的两处验证）。
- 单头发布（`termchart_full.hpp` / `single_include/`）尚未建立。

---

## 六、GUI 接入基础（L2 及以下）

`结构.md` 第二十八节定死了 GUI 栈的复用边界：**GUI 复用 `scene`（L2）及以下**，
`layout / widget / interaction / app` 四层在 GUI 下**重写**（设计选择，不是待解耦项）。
因此"为 GUI 留基础"只可能发生在 L0–L2，本仓目前在这一边界内的准备情况：

| GUI 需要的能力 | 本仓提供 | 位置 |
| --- | --- | --- |
| 矢量绘制 IR（GUI 侧的 `scene_renderer` 消费它） | `Scene` / 命令变体 / `SceneBuilder` / 访问者 | `scene/`（L2） |
| 知道"这份 Scene 多大"（决定画布与无效区） | `scene_bounds()`（线段取端点、圆弧取圆心±半径方框、文本按显示宽度、不计描边宽度） | `scene/bounds.hpp` |
| `core::Style` → GUI 样式（前景/背景真彩、背景是否透明） | `resolve_style()` / `resolve_color()`（**先按 ColorDepth 量化再取色**，保证 GUI 与终端观色一致；`None` 用默认色而非黑） | `color/resolve.hpp`（L1） |
| 位图显示、缩放、拼版、叠加 | `Bitmap`：`fill_rect` / `blit_over`（source-over）/ `scaled_to`（最近邻）/ `with_alpha` / `without_alpha` | `image/bitmap.hpp`（L2，header-only） |
| 图片与连续量的互转（GUI 的图片控件 ↔ 终端的盲文/密度预览） | `luminance_field()`（Rec.601，alpha 乘进去）/ `field_to_bitmap()`（阈值双色，可选 alpha 取自场值） | `image/convert.hpp`（L2，header-only） |
| 平滑/网格化渲染（同一份标量场，换光栅器就是换分辨率） | `Rasterizer` 接口 + 五种实现 + `RasterRegistry`（可注册自定义光栅器） | `raster/`（L2） |
| 坐标类型、几何、计时、调度、取消 | `Point`/`Rect`/`Rgb`/`Style`、`util/geom`、`util/timer`、`util/scheduler`、`util/task` | `core/`、`util/` |

**尚未开工**（GUI 侧真正的工作量）：`include/termchart_gui/` 与 `src_gui/` 共 25 个文件
（伞形头、`render/scene_renderer` `render/style_map`、`widget_tree/*`、`native_loop/*`、
`layout/*`（GUI 自己的约束/弹性/网格）、`backend_tk/*`）。规格把它们标为**平行项目**，
不阻塞库本体。

> 提醒：GUI 侧不要把终端路径的 `cell`/`SGR` 概念带进去。终端字符格是"低分辨率 +
> 颜色对"的表示法，GUI 的原子是**像素矩形 + 真彩色**；两者共用的应该是 `Scene`
> 与 `Style` 这类语义类型，而不是 `CellBuffer`。需要位图时用 `color::resolve` +
> `image::convert` 这条链，而不是去解析 ANSI 序列。
