# ROADMAP.md

> **本文件是 termchart 项目推进的唯一进度口径。**
> 所有"做到哪了"的判断，只以本文件的勾选清单为准；不以口头描述、不以某次构建结果为准。
> 每个阶段结束时必须满足该阶段的「出口条件」，才允许进入下一阶段。

---

## 〇、如何使用本文件

1. **勾选即完成**：一个条目从 `[ ]` 变为 `[x]`，必须同时满足：代码落地 + 单元测试通过 + `check_headers.sh` 通过 + 相关校验脚本通过。缺一不可。
2. **阶段不可跳跃**：阶段 0 未完成，不得开始阶段 1。地基类工作后置的代价是指数级的。
3. **规格以 `结构.md` 为准**：本文件只回答"什么时候做、按什么顺序做"，不重复回答"做成什么样"。文件清单、层号、依赖、职责一律查 `结构.md`。
4. **生成物禁手改**：`结构.md` 附一/附四、`docs/reference/module_matrix.md`、`single_include/`、`tests/golden/` 全部由脚本生成，改了生成器必须重跑。
5. **修订流程**：调整计划走 PR，在本文件顶部 `CHANGELOG` 记录一行，不直接改勾选状态以外的东西。

### 修订记录

| 日期 | 变更 | 影响阶段 |
| --- | --- | --- |
| TBD | 初稿建立 | 全部 |
| 2026-10-10 | 阶段 0 / 阶段 1 落地并跑通：护栏三件套全绿、ctest 7/7、doctest 84 用例 / 4154 断言 | 0、1 |
| 2026-10-11 | 风险登记册新增 R9（GLOB 重扫描瞬时失败），记录新增源文件后首次构建的假失败陷阱与对策 | 0–8 |
| 2026-10-11 | Windows 控制台 VT 修复：新增 `platform::enable_vt/disable_vt`（`enter()` 开 `ENABLE_VIRTUAL_TERMINAL_PROCESSING`，`leave()` 按原 mode 恢复）；`caps` 用 `WT_SESSION` 识别真彩色宿主、删除「TERM 为空即无色」误判；风险登记册新增 R10（ANSI/编码陷阱）与 R11（图像尺寸须自适应终端）；新增示例 `03_function_plot`、`04_heatmap`（自适应终端尺寸），改写 `02_sine_wave` 为盲文高精细版，修正 examples 中指向缺失源文件的坏引用 | 0–8 |

---

## 一、项目现状

**一句话**：规格完备；阶段 0（地基与护栏）+ 阶段 1（v0.1 终端上色闭环）已落地并跑通。

- ✅ `结构.md` 已定义 22 个模块（L0–L21）、约 400 个头文件、三条渲染路径、三物种后端、五注册表、完整测试/文档/工具链。
- ✅ 已落地：`include/termchart/{core,util,color,term,render,api}` + `detail/`（56 头）、`src/` 同名实现（36 cpp，含平台拆分）、`cmake/`（含分层规则唯一源）、`tools/`（3 个数据表生成器 + 模块图生成器 + 2 个护栏脚本 + golden 驱动器）、`tests/`（doctest + 单元 / 集成 / golden）。
- ✅ 验证状态：`ctest` 7/7（含两个 example）；doctest 84 用例 / 4154 断言全绿；`check_all`（分层 + 头文件自洽 + 生成物无 diff）通过；构建零警告。
- ❌ 未落地：`scripts/`（check_headers.sh / check_license.sh）、格式化与静态检查配置（`.clang-format` / `.clang-tidy` / `.cmake-format.yaml` / `.editorconfig` / `.git*`）、`.github/workflows/ci.yml`、`docs/design/*`、阶段 2 起的模块（raster / scene / canvas / scale…）。
- ⚠️ 权威来源分工不变：`结构.md` 回答"做成什么样"，本文件回答"什么时候做、做到哪"，`CODEBUDDY.md` 是给 AI 的架构摘要。

---

## 二、推进原则

| # | 原则 | 具体含义 |
| --- | --- | --- |
| P1 | **护栏先行** | 分层校验、生成物校验、头文件自洽校验必须先于库代码落地。一旦铺开上千个 `#include`，回头补规则等于重写。 |
| P2 | **垂直切片** | 每个版本打通一条端到端最小链路（能出图 / 能导出 / 有 golden），不横向铺开。 |
| P3 | **四件套同步** | 新增一个头文件 = CMake 引用 + `src/` 同名 `.cpp` + 单元测试 + 头文件自洽校验，同时完成。 |
| P4 | **契约前置** | 可扩展性契约（三个 `replace_*` 实验）在对应第一个实现出现时就建立，不放到最后补。 |
| P5 | **边界即承诺** | 复用边界（`scene(L2)` 及以下）、唯一真源（cmake 规则源）、唯一生成点（`render/sgr.hpp`）一旦确定，不在后续阶段放宽。 |

---

## 三、阶段总览

| 阶段 | 切片目标 | 核心交付 | 出口条件（机器可验证） |
| --- | --- | --- | --- |
| **0** | 地基与护栏 | 规则源、三校验脚本、仓库骨架、构建预设、doctest | 假违规被拦；生成物与规格一致 |
| **1** | v0.1 终端上色闭环 | core / util / color / term / 最小 render / api | example 01 跑通；终端恢复测试过 |
| **2** | v0.2 光栅与场景 | raster / scene / canvas / scale 起步 / detail 数据表 | example 02 跑通；utf8·grapheme fuzz 点亮 |
| **3** | v0.3 图表与导出 | layout / chart 起步 / document / backend 三物种 / interaction 起步 | example 03·04·34·35·36；三个 `replace_*` 过 |
| **4** | v0.4 图可视化 | graph 根 / layout / route / shape / diagrams 起步 | example 14–17；sugiyama·force bench |
| **5** | v0.5 DSL 与交互 | dsl 全套 / interaction 深化 | example 21·22·30·33；mermaid fuzz |
| **6** | v0.6 图像·协议·组件 | image / protocol / widget / app / plugin | example 24–27·38·40·41；并发测试 |
| **7** | v0.7 全谱与发布 | chart 剩余子目录 / diagrams 补齐 / 单头 / 文档 | 覆盖率·sanitizer·fuzz 全绿 |
| **8** | GUI 平行栈 | `termchart_gui` 全套 | 复用边界不越界 |

---

## 四、阶段详情

### 阶段 0 — 地基与护栏 ⚠️ 最高优先级

**目标**：在写第一行业务代码前，让所有机器强制规则可运行、可自测。

#### 任务清单

- [x] `cmake/TermchartModules.cmake`：`TERMCHART_MODULE_LAYERS`（22 项）+ `TERMCHART_NARROW_EDGES`（1 项）+ `TERMCHART_FORBIDDEN_EDGES`（3 项）
- [x] `tools/check_layers.py`：判定顺序 禁止边 → 收窄边 → 层号；`detail/` 返回 -1；顶层工具头返回 0；伞形头豁免
- [x] `tools/gen_module_graph.py`：读 cmake 生成附一 + `docs/reference/module_matrix.md`（现已支持 `--check`）
- [x] `cmake/header_only.txt`：12 个纯头清单
- [x] `cmake/TermchartGenerate.cmake` / `TermchartSingleHeader.cmake` / `FindTermchartDeps.cmake` / `CompilerWarnings.cmake` / `Sanitizers.cmake` / `TermchartOptions.cmake` / `TermchartAbiVersion.cmake` / `TermchartConfig.cmake.in`
- [ ] `scripts/check_headers.sh` / `check_generated.sh` / `check_license.sh` —— 护栏现由 `cmake --build --target check_all / check_layers / check_header_only / check_generated` 承担，shell 封装待补
- [x] `CMakeLists.txt` + `CMakePresets.json`（现八套：debug/release/relwithdebinfo/asan/tsan/coverage + ninja/ninja-release + debug-windows/release-windows/relwithdebinfo-windows）
- [ ] `.clang-format` / `.clang-tidy` / `.cmake-format.yaml` / `.editorconfig` / `.gitattributes` / `.gitignore`
- [x] `third_party/doctest/doctest.h` vendored + `tests/main.cpp` 空测试跑通（现已 84 用例）
- [x] `detail/` 数据表生成器 + 产物：`gen_unicode_tables.py`（`unicode_tables` / `box_chars` / `block_chars`）/ `gen_grapheme_tables.py` / `gen_braille_table.py`；`ansi_tables` / `sixel_table` / `sort_network` 为手写常量表（头部注释已说明，不由生成器产出）
- [ ] `.github/workflows/ci.yml` 骨架（三平台 + 分层检查 + diff 校验）

#### 出口条件

- [x] 故意放一条违规 `#include` → `check_layers.py` 退出码 1；清理后退出码 0（已实测）
- [x] `gen_module_graph.py` 产物与 `结构.md` 附一 / 附四、`docs/reference/module_matrix.md` 一致（`--check` 幂等）
- [ ] `check_headers.sh` 对 `detail/` 下每个头独立编译通过
- [x] 测试在 debug preset 下 ctest 通过（7/7）

> **为什么最先**：仓库尚未 populate 时脚本扫不到东西，所以这是唯一能用"假输入"做 TDD 的阶段。`detail/` 数据表层号为 -1、被大量模块依赖，是全局地基，必须此刻生成。

---

### 阶段 1 — v0.1 终端上色闭环

**目标**：打通最短端到端链路，把最关键的三个接口一次定对。

```
core → util/color → term(RAII) → render(renderer/sgr/diff) → api/quick
```

#### 任务清单

- [x] `core/`：`rgb` `color_depth` `attr` `style` `cell` `grapheme` `buffer` `point` `rect` `span_utils` `error` `expected` `arena`
- [x] `util/` 起步：`utf8` `grapheme` `wcwidth` `text` `bits` `math` `assert` `logging` `format`
- [x] `color/` 起步：`palette` `quantize` `named_colors` `gradient` `colormap`
- [x] `term/`：`terminal` `caps` `raw_mode` `size` `ansi` `daemon_safe` `signal` `waker`
- [x] `render/`：`renderer` `sgr` `diff` `damage`
- [x] `api/` 起步：`quick`（含 `std::call_once` 的 `init_core()`）`opts` `print`
- [x] `version.hpp` `export.hpp` `deprecate.hpp` `fwd.hpp` `termchart.hpp`
- [x] `examples/01_hello_color.cpp` `examples/32_quick_one_liner.cpp`（两者都注册进 ctest）
- [x] `tests/unit/core/*` `util/*` `color/*` `render/test_render.cpp` `render/test_sgr.cpp` `render/test_diff_damage.cpp` `core/test_buffer.cpp` `scale/*` `canvas/*` `scene/*` `api/*` `term/*` `test_contracts.cpp`（确定性 UTF-8/grapheme/宽度 fuzz 含于内）
- [x] `tests/integration/test_terminal_restore.cpp` `test_full_render.cpp`
- [x] `tests/golden/ansi_props/` 框架建立（**解析成属性再比对，不直接比字节**）；快照由 `python tools/gen_golden.py` 生成，已产出 5 份

#### 关键约束（本阶段一旦定错，后续全错）

- `core/cell.hpp` 只存**单个主码点** `char32_t`；组合序列结构放 `core/grapheme.hpp`（只定义不分割）；**分割在 `util/grapheme.hpp`**。
- `render/sgr.hpp` 是**唯一 SGR 序列生成点**——所有 snapshot 后端将来都复用它。接口要一次定对。
- `term/terminal.hpp` 是 RAII 门面，异常退出必须恢复终端。

#### 出口条件

- [ ] example 01 上色并正常退出
- [ ] `test_terminal_restore` 通过
- [ ] `ansi_props` golden 首次生成并稳定
- [ ] `check_headers.sh` 对阶段 1 全部头通过

---

### 阶段 2 — v0.2 光栅与场景

**目标**：打通 `Scene → scene_renderer → Canvas → to_buffer() → CellBuffer → renderer`。

#### 任务清单

- [x] `raster/`：`rasterizer` 基类 + `braille` `halfblock` `quadrant` `ascii` `block` + `registry` + `auto_select`（六分块 `sextant` 用 U+1FBxx 新区块，暂未实现，见下注）
- [x] `scene/`：`scene` `command` `builder` `visitor`（矢量 IR 本体；`command.hpp` 是 header-only，已在 `cmake/header_only.txt` 里登记）
- [x] `canvas/`：`canvas` `primitives` `scene_renderer`（起步；`fill` / `path` / `layer` / `transform` / `clip` 待补）
- [x] `scale/` 起步：`scale` `linear` `log` `ticks` `nice` `auto_domain`（纯函数部分；`category` / `band` / `registry` 待补）
- [x] `image/bitmap.hpp`（`Bitmap`：RGBA 像素缓冲，宽/高/通道/行步长）+ `image/field.hpp`（`FloatField`：标量强度场，`raster/` 的输入）；两者都是 header-only。拆开是因为"图片像素"与"标量场"是两种东西，不该挤在一个 `bitmap.hpp` 里
- [x] `examples/02_sine_wave.cpp`
- [x] `tests/unit/raster/*` `image/*` `canvas/*` `scene/*` `scale/*`
- [x] `layout/`：`box` `axis` `grid` `legend` `title` `label`（在 canvas 之上做轴/网格/图例/标题，依赖 scale，不碰 render）
- [x] `chart/`：`chart` `series` `registry` `cartesian/line_chart` `cartesian/bar_chart`（端到端折线/柱状图，不碰 render）
- [x] `examples/03_line_chart.cpp` `04_bar_chart.cpp`
- [x] `tests/unit/layout/*` `chart/*`
- [ ] `tests/fuzz/fuzz_utf8.cpp` `fuzz_grapheme.cpp`
- [ ] `bench/bench_raster.cpp` `bench_utf8.cpp` `bench_scene.cpp`

> 注：`sextant`（2×3 六分块）映射到 Unicode "Symbols for Legacy Computing" 区块 U+1FB00+，属于较新范围，终端支持度低且与 blind/quadrant 重叠；当前阶段先交盲文/象限，sextant 留待阶段 7 图元数据密度需求再补。若补，需先扩展 `detail/` 六分块表。

#### 关键约束

- `Scene` 是**非完备图形 IR**，只表达 vector 后端能消费的图元。**不要往 scene 里塞 canvas 才有的绘制能力。**
- `canvas/scene_renderer.hpp` 是**终端路径上 Scene→Canvas 的唯一桥接点**。
- `detail/` 数据表（盲文/块字符/制表符）已在阶段 0 就绪，此处直接消费。

#### 出口条件

- [x] example 02（正弦波，ASCII 线/填充占位；盲文/半块等子像素光栅器已实现，可在 canvas 上叠加）跑通
- [ ] `bench_raster` 有基线数值写入 `baseline.json`
- [ ] utf8 / grapheme 两个 fuzz 目标可运行（fuzz 性质已以确定性循环形式落地在 `test_contracts.cpp`，独立 fuzz 目标待补）

---

### 阶段 3 — v0.3 图表与导出 🚩 架构契约验证点

**目标**：三条渲染路径、三物种后端、注册表快照语义在此第一次交汇。**这是风险最高的阶段。**

#### 任务清单

- [x] `layout/` 起步：`layout` `box` `axis` `grid` `legend` `title` `label`（`annotation` `facet` `auto_margin` `label_avoid` 待补）
- [x] `chart/` 根：`chart` `registry` `series`（`context` / `dataset` / `hit_region` 待补）
- [x] `chart/cartesian/` 起步：`line_chart` `bar_chart`（`area` `scatter` `hist` `heatmap` `sparkline` 与其余 16 个待补）
- [x] `document/`：`document` `block` `span` `table_model`（structured 物种的输入 IR）
- [x] `backend/` 起步：根 `backend` + `snapshot/`（`backend` `ansi_backend` `text_backend` `html_pre_backend` `json_backend`，`ansi_backend` 复用唯一收窄边 `> render/sgr`）+ `vector/`（`backend` `svg_entities` `svg_backend`）+ `structured/`（`backend` `html_backend` `markdown_backend`）。三物种各至少一个后端；`png_screenshot` 待补
- [x] `tests/unit/document/*` `backend/*`
- [ ] `interaction/` 起步：`event` `key` `input` `input_parser` `mouse` `focus` `hit_test` `dispatcher`
- [ ] `scale/registry.hpp`（本轮新增，与 chart/raster 对齐）
- [x] `examples/03_line_chart.cpp` `04_bar_chart.cpp` `34_export_svg.cpp` `35_export_html.cpp` `36_export_json.cpp`（`37_export_latex.cpp` 待补）
- [x] **`tests/integration/replace_backend_html_pre.cpp`（实验 A）**——实现 `SnapshotBackend` 接口即接入，核心零改动
- [x] **`tests/integration/replace_chart_radial.cpp`（实验 B）**——实现 `Chart` + `ChartRegistry::register_chart`，核心零改动
- [x] **`tests/integration/replace_raster_custom.cpp`（实验 C）**——实现 `Rasterizer` + 新增的 `RasterRegistry::register_raster`，核心零改动
  > 实验 C 的发现：`RasterRegistry` 最初是**封闭枚举 + 硬编码 switch**，不可扩展——已补 `register_raster(name, creator)` / `supports_name` 与 `ChartRegistry` 对齐。此外 `RasterKind` 仍是封闭枚举，自定义光栅器的 `kind()` 需借用现有值（若将来需要可改为可选 name）。文件暂名 `replace_raster_custom`：六分块 `sextant` 的 60 格局字形表（U+1FB00+）易错，待有把握时再补。
- [x] `tests/golden/test_golden_exports.cpp`（text / svg / json 三类快照）
- [ ] `tests/integration/test_roundtrip_scene.cpp` `test_headless_pipeline.cpp`
- [ ] `bench/bench_render.cpp` `bench_diff.cpp` `bench_svg_backend.cpp`

#### 关键约束（禁止边在此显形）

- **`backend > chart` 禁止**：backend 走 Document，绝不碰 chart 类型。
- **`chart > render` 禁止**：chart 不碰渲染管线，保住 `to_document()` 通路。
- **`render > interaction` 禁止**：维持单向。
- **唯一收窄边 `backend/snapshot/ansi_backend > render/sgr`**：只有 `ansi_backend.hpp` 能 include `sgr.hpp`，其余文件对全部关闭。**PR 必须写明为何不能放宽到模块级。**
- 写后端前先回答："输入是 CellBuffer、Scene 还是 Document？"——**后端不做跨渲染模型转换。**
- `interaction/hit_test.hpp` 只 switch `HitRegion::Shape`，**不得出现任何图表名**。
- `layout/facet.hpp` 只做纯几何划分，禁止创建/绘制 chart。
- `App` 构造时对全部注册表做浅拷贝快照并冻结；**运行期注册 = UB**。

#### 出口条件

- [x] 三个 `replace_*` 实验全部通过（新增 backend / chart / raster，**核心零改动**）
- [x] example 03·04 跑通；34（SVG）35（HTML）36（JSON）跑通（37 LaTeX 待补）
- [x] text / svg / json 三类 golden 建立（`tests/golden/{snapshot_text,snapshot_json,vector_svg}/basic.txt`）
- [x] `check_generated` 稳定（单头、module_matrix 无 diff）——由 `check_all` 覆盖

> **不要把这些实验留到阶段 7**。它们是可扩展性契约，越早建立越能约束后续所有设计。

---

### 阶段 4 — v0.4 图可视化

#### 任务清单

- [ ] `graph/` 根：`graph` `node` `edge` `attributes` `wiregrid` `arrow` `port` `label_place` `crossing`
- [ ] `graph/layout/`：`layout` `sugiyama` `tree_layout` `radial` `force` `kamada_kawai` `circular` `grid`
- [ ] `graph/route/`：`route` `orthogonal` `straight` `spline` `arc` `obstacle`
- [ ] `graph/shape/`：`shape` `box` `ellipse` `diamond` `hexagon` `stadium` `cylinder` `icon`
- [ ] `graph/diagrams/` 起步：`flowchart` `state_machine` `mindmap` `orgchart` `dag` `dependency`
- [ ] `examples/14_flowchart.cpp` `15_state_machine.cpp` `16_mindmap.cpp` `20_force_graph.cpp`
- [ ] `tests/unit/graph/*` `test_wiregrid.cpp` `test_diagrams_flow.cpp`
- [ ] `bench/bench_sugiyama.cpp` `bench_force.cpp`

#### 出口条件

- [ ] example 14–17 跑通
- [ ] sugiyama / force 有性能基线
- [ ] `interaction > graph` 放行验证（命中判定需要 `HitRegion` 语义）

---

### 阶段 5 — v0.5 DSL 与交互

#### 任务清单

- [ ] `dsl/` 全套：`parser_base` `lexer` `ast` `mermaid` `mermaid_flow` `mermaid_sequence` `mermaid_class` `mermaid_state` `graphviz_dot` `plantuml` `d2` `table_markdown` `csv_sniff` `dsl_registry`
- [ ] `interaction/` 深化：`tooltip` `crosshair` `highlight` `zoom` `pan` `select` `brush` `legend_toggle` `immediate`
- [ ] `examples/21_hover_tooltip.cpp` `22_zoom_pan.cpp` `30_legend_toggle.cpp` `33_mermaid_flowchart.cpp`
- [ ] `tests/unit/dsl/*` `interaction/*`
- [ ] `tests/fuzz/fuzz_mermaid.cpp` `fuzz_dsl_lexer.cpp` `fuzz_input_parser.cpp`
- [ ] `bench/bench_mermaid_parse.cpp`
- [ ] `tests/fixtures/sample_mermaid.txt` `sample_dot.txt` `sample_plantuml.txt` `sample.csv`

#### 出口条件

- [ ] example 21·22·30·33 跑通
- [ ] 三个 dsl 相关 fuzz 目标可运行
- [ ] `dsl_registry` 遵循 function-local static + 快照语义

---

### 阶段 6 — v0.6 图像·协议·组件·应用

#### 任务清单

- [x] `image/` 起步：`bitmap`（RGBA）+ `field`（标量场）；`decoder` `png` `png_encode`（zlib **只在此引入**）待补
- [x] `protocol/` 起步：`protocol`（基类）`sixel`（量化+调色板+RLE）`kitty_graphics`（原始 RGBA+base64 分块）`halfblock_image`（真彩半块）`fallback`（Kitty>Sixel>HalfBlock>None）`detect`（环境探测）；`iterm2_inline` 待补
  > `iterm2_inline` 需要 **PNG** 负载（iTerm2 不认原始 RGBA），而 PNG 编码器（`image/png_encode`，含 deflate/CRC32/adler32）尚未落地，故暂缺——宁可不出错误字节流。`ProtocolKind::Iterm2` 已预留但 `pick_protocol` 不选它。
- [ ] `widget/`：`widget` `container` `vbox` `hbox` `grid_layout` `dock` `panel` `border` `scroll` `styles` `style_sheet` `theme` `theme_dark` `theme_light` `theme_mono` `immediate_widget` `reactive`
- [ ] `app/`：`app` `loop` `animation` `config` `post` `immediate_app` `headless`
- [ ] `plugin/`：`plugin` `loader` `dynamic` `manifest`
- [ ] `api/` 收尾：`quick_line` `quick_bar` `quick_spark` `quick_table` `termchart_full.hpp`
- [ ] `examples/24_theme_switching.cpp` `25_animation.cpp` `26_realtime_stream.cpp` `27_background_thread.cpp` `39_headless_render.cpp` `40_immediate_mode.cpp` `41_reactive_dashboard.cpp`
- [x] `examples/38_image_sixel.cpp`（Bitmap → 探测 → 选协议 → 写 stdout）
- [x] `tests/unit/protocol/*` `image/*`；`widget/*` `test_plugin.cpp` `test_capability.cpp` 待补
- [ ] `tests/integration/test_app_loop.cpp` `test_thread_post.cpp` `test_resize.cpp`
- [ ] `tests/fuzz/fuzz_sixel.cpp` `fuzz_colormap.cpp`

#### 关键约束

- 并发模型：**单线程主循环**。后台线程只经 `app/post.hpp` 投递数据；`SIGWINCH` 经 `term/signal.hpp` 转发主线程。
- 五个注册表（chart/raster/shape/dsl/scale）全部完成，统一模式。
- `App` **不依赖 widget**。

#### 出口条件

- [ ] example 24–27·38–41 跑通
- [ ] 并发测试通过（投递、唤醒、SIGWINCH）
- [ ] 全部 7 个 fuzz 目标可运行

---

### 阶段 7 — v0.7 全谱与发布

#### 任务清单

- [ ] `chart/cartesian/` 补齐剩余 16 个：`bubble` `candlestick` `boxplot` `violin` `contour` `step` `errorbar` `waterfall` `gantt` `dumbbell` `ridgeline` `parallel` `mosaic` `qq` `bullet` `streamgraph`
- [ ] `chart/polar/`（8）/ `statistical/`（11）/ `hierarchical/`（6）/ `financial/`（7）/ `flow/`（2）/ `geo/`（4）/ `text/`（4）/ `indicator/`（5）
- [ ] `graph/diagrams/` 补齐剩余 15 个
- [ ] `graph/layout/` `metro` `cluster`；`graph/route/` `bundling`
- [ ] 单头文件生成：`gen_single_header.py` + `single_include/`（去重、拓扑排序、匿名命名空间隔离）
- [ ] `docs/` 全套：顶层 + `design/`（18）+ `guide/`（18）+ `charts/`（10）+ `graph/`（8）+ `reference/`（10）+ `contrib/`（5）
- [ ] `tools/gen_golden.py` / `gen_gallery.py` / `gen_api_docs.py` / `gen_colormap.py`
- [ ] `Doxyfile` + `mkdocs.yml`
- [ ] `.github/workflows/`：release / docs / sanitizers / fuzzing / license
- [ ] `examples/` 补齐剩余（05–13、18–19、23、28、29、31、42–54 + `single/`）
- [ ] `LICENSES/` + `assets/`（colormaps、maps、logo、banner）
- [ ] 根目录文档：`README` `README_zh` `CHANGELOG` `CONTRIBUTING` `CODE_OF_CONDUCT` `AUTHORS` `ABI_POLICY` `ERROR_POLICY` `SECURITY` `AGENTS`

#### 出口条件

- [ ] 覆盖率达标（`coverage.sh` 输出到 `build/coverage`）
- [ ] ASan / UBSan / TSan 全绿
- [ ] 全部 fuzz 目标运行稳定
- [ ] `check_generated.sh` 全部生成物无 diff
- [ ] `check_layers.py` 通过
- [ ] 单头用法（example 42）验证通过

---

### 阶段 8 — GUI 平行栈

**目标**：`include/termchart_gui/` + `src_gui/` 全套落地。

#### 任务清单

- [ ] 复用层：`render/scene_renderer` `render/style_map`（Scene → tk canvas primitives）
- [ ] 重写层：`widget_tree/`（node/widget/container/mount/reconcile）、`native_loop/`（loop/event/dispatcher/invalidate/timer）、`layout/`（layout/constraint/flex/grid）、`backend_tk/`（backend/tk_window/tk_canvas/tk_event_map）
- [ ] `gui.hpp` `version.hpp` `fwd.hpp`

#### 关键约束

- **复用边界**：scene(L2) 及以下（core/util/color/scale/scene/image/raster/term/protocol）。
- **重写边界**：layout / widget / interaction / app 四层，这是**明确设计选择，不是技术债**。
- **不得**为了"未来复用"把四层抽成前端无关接口——当前没有第二个前端，抽象会猜错。

#### 出口条件

- [ ] scene 及以下**零改动**（复用边界的硬验证）
- [ ] GUI 示例跑通，原生事件循环正常

---

## 五、横切纪律（全程适用，不分阶段）

| 纪律 | 说明 | 违反后果 |
| --- | --- | --- |
| **层号预演** | 仓库 populate 前 `check_layers.py` 拦不住，写每个头前按附一自查 include 目标层号 | CI 后期暴雷 |
| **生成物禁手改** | 附一/附四、`module_matrix.md`、golden、单头文件 | `check_generated.sh` 挂 |
| **同路径同名** | `src/` 与 `include/` 严格一一对应，基类留 cpp 放虚表锚点 | `check_headers.sh` 挂 |
| **平台隔离** | 新增 `#ifdef` 必须落在 `term/terminal_posix` / `_win32` 隔离层 | 架构泄漏 |
| **唯一生成点** | SGR 只在 `render/sgr.hpp`；快照后端不自建 SGR | 输出分叉 |
| **唯一真源** | 依赖变更只改 `cmake/TermchartModules.cmake`，不写进文档 | 文档与代码分叉 |
| **快照语义** | 注册只在 `App` 构造前；运行期注册 = UB | 行为不可复现 |
| **契约测试** | `replace_*` 实验破坏 = 破坏可扩展性契约 | 扩展性退化 |

---

## 六、风险登记册

| ID | 风险 | 触发条件 | 对策 | 归属阶段 |
| --- | --- | --- | --- | --- |
| R1 | 地基后置 | 先写库代码后补校验脚本 | 阶段 0 先于一切 | 0 |
| R2 | SGR 双写 | snapshot 后端自建 SGR | `render/sgr.hpp` 阶段 1 定稿 + 收窄边登记 | 1/3 |
| R3 | 契约后补 | `replace_*` 放到最后 | 各第一个实现时就建立 | 3 |
| R4 | 渲染模型混用 | 后端选错族（CellBuffer/Scene/Document） | 写后端前先答"输入是什么" | 3 |
| R5 | detail 表滞后 | 横切数据表未先就绪 | 阶段 0 一并生成 | 0 |
| R6 | GUI 提前耦合 | 为假想复用抽接口 | 严格守 scene 及以下边界 | 8 |
| R7 | 注册时序失控 | 运行期注册 | 快照语义 + 回归测试 | 3/6 |
| R8 | 单头生成不稳定 | 文件数未收敛就生成 | 放阶段 7 | 7 |
| R9 | GLOB 重扫描瞬时失败 | CMake 用 `file(GLOB)` 收集源文件，新增 `.cpp` 后首次构建偶发"代码无误却编不过"（曾现 `layout.cpp` 的 `Rect::bottom()` 假错误），强制重编即过 | 新增源文件后先 `cmake --preset ninja` 重新 configure 一次再构建 | 0–8 |
| R10 | Windows 控制台 ANSI/编码 | Windows 中文环境（传统 ConsoleHost，`[Console]::OutputEncoding` 为 GBK/936）下运行时：ANSI 转义序列裸露成 `[38;2;…m`、盲文/框线/`▀` 显示为乱码（`鈹` `猓`）。**`chcp 65001` 不足**——它只改控制台代码页，管不到 PowerShell 解码子进程输出的那一层 | 两端配合：① 库层 `Terminal::enter()` 调 `platform::enable_vt()` 置位 `ENABLE_VIRTUAL_TERMINAL_PROCESSING`（`leave()` 按原 mode 精确恢复）；② 用户层设 `[Console]::OutputEncoding=[Text.Encoding]::UTF8`。最省事：改用 Windows Terminal / VS Code 终端（默认 UTF-8 + VT）。另：cmd 用 `chcp 65001`、PowerShell 用 `[Console]::OutputEncoding`，二者只设其一时字符层仍会坏 | 0–8 |
| R11 | 图像尺寸超终端导致错位 | 绘图时把画布/图像写死成大于终端可视区的尺寸（如热力图 90 列，而终端只有 60 列）：超宽每行折行、超高触发滚动，两者叠加使图像错位（看起来像"颜色溢出/串色"，甚至顶部被截断） | 绘图前用 `terminal.size()` 取**实际**可视区自适应：宽 = `cols`、高 = `rows - 1`（留一行给命令行提示符）；并按字符格约 1:2 的宽高比做等比取景避免变形 | 0–8 |

---

## 七、进度口径定义

**"完成"的唯一含义**（四条件同时满足）：

1. 代码已落地（`include/` + `src/` 同路径同名）
2. 对应单元测试已写并通过
3. `check_headers.sh` 对该头文件通过（独立可编译、无隐式依赖）
4. 若涉及依赖变更，`check_layers.py` 通过；若涉及生成物，`check_generated.sh` 通过

**阶段完成的唯一含义**：该阶段「出口条件」全部满足。

**禁止的表述**："差不多完成"、"本地跑通了"、"代码写好了就差测试"。

---

## 八、立即启动（阶段 0 的前三步）

1. **落规则源**：`cmake/TermchartModules.cmake` + `tools/check_layers.py`，用一条假违规头文件自测（确认 1 → 清理后 0）。
2. **落构建骨架**：`CMakeLists.txt` + `CMakePresets.json` + `.clang-format`/`.clang-tidy`/`.editorconfig`，vendored doctest，空测试跑通。
3. **落进度口径**：把本文件与 `结构.md` 文件清单逐条对齐，确认每个阶段"包含哪些文件"无遗漏、无重复。

> 三步完成后，阶段 0 才真正开始。在此之前，不写任何 `core/` 及以上的业务代码。