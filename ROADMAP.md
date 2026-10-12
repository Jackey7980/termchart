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
| 2026-10-12 | **metro 已知红清零（graph 6 缺口收口）**。根因与修法：①`layout_metro` 的 3 例期望未含实现的"step 抬升到最大框宽高"——测试改为压小框（5×2）使有效 step 恰为 opts.step；②`route_metro` 的真 bug：恰为 45° 对角时 `side_towards` 主选锚点被自身节点框堵死，直连/45°步进/L形全失败后落到"直线兜底"（不查障碍 → 穿框 + 非 45° 段）。修法是**锚点候选对**：主选优先、4 边全枚举（16 对），每对走完 直连→45°步进→L形 阶梯，全败才退直线。**最终验证**：`ctest` **42/42 全绿**、doctest **757 用例 / 190765 断言、0 失败**；四护栏全绿（275 头 / 235 实现）。graph 规格缺口（metro/cluster/bundling/obstacle/dag/dependency）就此闭合 | 2–4 |
| 2026-10-12 | **graph 6 缺口落地（并行包）**：`layout/metro`（链分解 + 45° 之字 + 栅格路由：直连→45°步进→L形→直线兜底）、`layout/cluster`、`route/bundling`、`route/obstacle`、`diagrams/dag`（含环检测）、`diagrams/dependency`，3 个测试文件 17 用例。**最终验证**：`ctest` 全绿（除 metro 3 用例）、doctest **756 用例 / 190874 断言、753 通过**；`check_header_only` 275 头 / 235 实现。**已知红（3 用例 / 23 断言，均在 metro）**：①测试期望用 opts.step=8，未考虑实现文档化的"step 抬升到最大框宽高"（测试需按有效 step 断言）；②metro 路由 45°/穿框检查未过，需跑实现定位（可能实现有真 bug）。这 3 例是下一切片的第一件事 | 2–4 |
| 2026-10-12 | **chart 全子目录铺开（68 文件）**：6 个并行包 + 主线收口。cartesian 23/23（补 16 图种）、polar 8/8、statistical 11/11、hierarchical 6+1、financial 7/7、flow 2/2、geo 4/4、text 4/4、indicator 5/5——规格 9 子目录全部齐备。断点恢复过程中修掉 2 处遗留编译错误（choropleth 的 Color/Style 误用、wordcloud 重复定义）、补齐 drawdown/yield_curve 两个缺失实现；statistical/financial 各缺的 7+3 个图种与 4 个缺失 cpp 由收尾包补齐。测试：chart 九个子目录的测试文件**全部接入**（polar/statistical/financial/hierarchical/flow/geo/text/indicator/cartesian×3，**ctest 42/42、doctest 739 用例 / 190594 断言全绿**；此前登记的 13 处"已知红"实为中断瞬间的中间态二进制，干净重跑即全绿）。横切：`.github/workflows/ci.yml` 新增（护栏三件套 + ctest 双平台必过）。 | 3 |
| 2026-10-12 | **阶段 5 收口：interaction 深化（5 个缺失组件）+ example 30**。新增 `highlight`（高亮放大：锚点居中、跨度 /magnify，复用 zoom 的锚点性质措辞）、`select`（框选状态机：begin/update/end/cancel，矩形归一化、payloads_inside 保持输入顺序）、`brush`（刷选：半径吸附、首次顺序去重）、`legend_toggle`（图例点击切换：id→可见性，payload 0 永不生效，click_at 复用 topmost_hit）、`immediate`（Event 流路由进 Viewport/BoxSelect 的最小粘合层）。新增 `examples/30_legend_toggle.cpp`（三帧脚本：全可见 → 点 sin 隐藏 → 恢复 sin 隐藏 cos，图例 `[x]` 标记与状态同源）。两个并行工作包（hl-select / brush-imm）+ 主线（legend_toggle + example 30 + 收口），共享文件仍只有 tests/CMakeLists.txt 与 examples/CMakeLists.txt 的追加行。**最终验证**：`ctest` **42/42 全绿**（doctest **601 用例 / 190214 断言**），四护栏全绿（`check_header_only` 205 头 / 165 实现）。阶段 5 出口条件（example 21·22·30·33 / 三个 dsl fuzz / dsl_registry 快照语义）**全部满足** | 5 |
| 2026-10-12 | **L17 `dsl` 第一阶段（6/14 文件）**：`parser_base` / `lexer` / `ast` / `dsl_registry` / `mermaid` / `mermaid_flow` + 单测 + `sample_mermaid.txt` + example 33。验证：独立构建目录 `build/dsl-ninja` 构建零失败、`ctest` 32/32、四护栏全绿 | 5 |
| 2026-10-12 | **L17 `dsl` 开始落地（第一批 6/14 文件）**：`parser_base`（递归下降框架：peek/advance/match + 首个诊断定位）/ `lexer`（箭头感知切分、UTF-8 标签、注释前缀可配、非法字节不阻塞游标）/ `ast`（中立模型 + `to_graph` 唯一适配点）/ `dsl_registry`（function-local static + 快照语义 + 幂等内置登记）/ `mermaid`（首行指令识别与分发，未实现图种返回 `NotImplemented`）/ `mermaid_flow`（头部/方向、8 种节点形状、9 种箭头、两种边标签写法、边链、subgraph 归属）。配套：`tests/unit/dsl/test_dsl_mermaid.cpp`（15 用例 / 153 断言）、`tests/fixtures/sample_mermaid.txt`（生成器 `tools/gen_dsl_samples.py`，已接入 `generate_all` / `check_generated`）、`examples/33_mermaid_flowchart.cpp`（解析→Graph→复用 `render_flowchart`）。本轮在独立构建目录 `build/dsl-ninja` 验证：构建 96 目标零失败、`ctest` **32/32 全绿**（doctest 513 用例 / 189524 断言）、四护栏全绿。**未做（明确留给后续切片）**：`mermaid_sequence`/`mermaid_class`/`mermaid_state`、`graphviz_dot`、`plantuml`、`d2`、`table_markdown`、`csv_sniff`，以及 `fuzz_mermaid`/`fuzz_dsl_lexer`/`bench_mermaid_parse`、`sample_dot.txt`/`sample_plantuml.txt`/`sample.csv` | 5 |
| 2026-10-10 | 阶段 0 / 阶段 1 落地并跑通：护栏三件套全绿、ctest 7/7、doctest 84 用例 / 4154 断言 | 0、1 |
| 2026-10-11 | 风险登记册新增 R9（GLOB 重扫描瞬时失败），记录新增源文件后首次构建的假失败陷阱与对策 | 0–8 |
| 2026-10-11 | Windows 控制台 VT 修复：新增 `platform::enable_vt/disable_vt`（`enter()` 开 `ENABLE_VIRTUAL_TERMINAL_PROCESSING`，`leave()` 按原 mode 恢复）；`caps` 用 `WT_SESSION` 识别真彩色宿主、删除「TERM 为空即无色」误判；风险登记册新增 R10（ANSI/编码陷阱）与 R11（图像尺寸须自适应终端）；新增示例 `03_function_plot`、`04_heatmap`（自适应终端尺寸），改写 `02_sine_wave` 为盲文高精细版，修正 examples 中指向缺失源文件的坏引用 | 0–8 |
| 2026-10-11 | **深度代码审计（L0–L21 真实落地度）**：逐模块 LOC 统计 + 桩标记扫描 + 崩溃桩扫描 + 四护栏重跑。结论：L0–L16 全部有真实实现（合计约 16.7k 行，生成代码 0 行混入源树：graph 1636 / term 1432 / backend 1134 / interaction 814 / util 1192 / core 1175 / render 721 / raster 689 / color 674 / chart 577 / protocol 492 / canvas 435 / layout 429 / scene 399 / scale 449 / document 271 / image 157 header-only / api 366）；L17 dsl / L18 widget / L19 plugin / L20 app 为零代码空洞（代码与构建脚本均无引用）；L21 api 有实现。护栏全绿（check_layers、check_header_only 135头/101实现、check_headers 135头、4 生成器 --check、ctest 全绿）。**关键发现**：审计当时 `canvas/scene_renderer.cpp` 的 Scene→Canvas 桥接尚用占位字符（`kStrokeChar=U'*'`、ImageOp 用 `kMediumShade`），即 R12 缺口。**该缺口已于后续提交收口**：描边改走 braille 2×4、填充改走 halfblock 1×2，golden `scene_raster/` 锁外观；`chart`/`graph` 示例（03/04/14/15/16/20）本就直连 `canvas.draw_line`/`draw_text` + 显式字符，从未出占位符（实测 `*` 计数为 0）。桩标记 5 处（scene_renderer 仅剩 ImageOp、layout/internal.hpp、canvas.cpp、api/quick.cpp、core/error.cpp）均为文档化占位、非隐藏空函数；崩溃桩扫描无异常（仅注册表 return nullptr）。登记风险 R12（现已收口），并修正「现状」对 graph 的漏判，以及 ctest/doctest 计数（早期构建子集曾写 16/16 / 279、17/17 / 303 / 185209，均陈旧；2026-10-11 及本轮干净重跑 `ctest --preset ninja` 确权为 24/24 / 335 用例 / 185558 断言，与 CODEBUDDY 一致）。 | 0–8 |

---

## 一、项目现状

**一句话**：规格完备；逐模块 LOC 审计确认 **L0–L16 全部有真实实现、L21 api 有实现**，但 **L17 dsl / L18 widget / L19 plugin / L20 app 是零代码空洞**（代码与构建脚本均无引用）；阶段 6 的 image / protocol 部分提前落地。**阶段 4 图可视化已接通**：`graph` 根（节点/边/元数据/连线栅格/箭头/端口/标签避让/交叉计数）+ 布局（网格/分层/根树/力导向）+ 路由（直线/正交）+ 形状（矩形/圆角/菱形）+ 图种（流程图/状态机图/思维导图/节点-连线），示例 14·15·16·20 跑通。代码量真实（include+src 合计约 16.5k 行，生成代码 0 行混入源树）；**跨层缝 R12 已收口**——`canvas/scene_renderer.cpp` 的描边改走 braille 2×4 子像素、填充改走 halfblock 1×2 实心，`Scene → Canvas` 现在出真图，外观由 golden `scene_raster/basic.txt` 锁住。

- 已完成：`结构.md` 已定义 22 个模块（L0–L21）、三条渲染路径、三物种后端、注册表语义、完整测试/文档/工具链。
- 已落地模块真实代码量（include/ + src/ 的 cpp+hpp，排除生成物 `detail/`，单位行）：`graph` 2467 · `term` 1480 · `backend` 1134 · `util` 1192 · `core` 1175 · `interaction` 814 · `render` 721 · `raster` 689 · `color` 674 · `chart` 577 · `protocol` 537 · `layout` 467 · `scale` 449 · `canvas` 614 · `scene` 399 · `api` 366 · `document` 271 · `image` 157（header-only）。**生成代码 0 行**：**五个**生成器产物（unicode / grapheme / braille / **turbo** / module_graph）落在 `detail/` 不参与此统计，未灌入主源树（`detail/` 合计 2209 行，含 `ansi_tables`/`sixel_table`/`sort_network` 三份手写表）；`assets/colormaps/turbo.dat` 是数据而非代码。
- 桩标记扫描（5 处，均为文档化占位，非隐藏空函数）：`scene_renderer.cpp`（**仅剩 ImageOp**——Scene 里没有像素数据）、`layout/internal.hpp`（占位级线/网格/刻度字符）、`canvas.cpp` / `api/quick.cpp`（宽字符续接占位，属正常渲染逻辑）、`core/error.cpp`（"not implemented yet" 为不支持操作的报错文案）。**R12 收口后 `kStrokeChar = U'*'` 已从代码中删除**，描边改走真 braille 子像素。**已澄清一处误报**：`graph/wiregrid.cpp` 是**真的占用栅格**（被 `route_edges` 消费、有原子占用语义与测试），此前的"占位"是关键词误读。崩溃桩扫描（`std::terminate` / `abort` / `assert(false)`）**无异常**，仅注册表查不到时 `return nullptr`，属正常 not-found 返回。
- 验证状态（2026-10-11 干净重跑 `ctest --preset ninja` + `ctest --preset fuzz` + `check_all`）：`check_layers.py` 通过；`check_header_only.py` 通过（**186 头 / 146 实现**）；`check_headers` 通过（186 头全通过）；**五个**生成器 `--check` 一致（结构.md / module_matrix / unicode / grapheme / braille / **turbo** 无 diff）；**`ctest` 31/31 全绿**；**doctest 497 用例 / 189357 断言全绿**；golden 9 份；**`ctest --preset fuzz` 13/13**（6 个 fuzz 目标的 `--self-test`，语料 24 条共享 + PNG 专项 6 条），并实测 6×20000 组随机输入（seed=7/11）无崩溃。全量重编 **0 warning**（vendored `doctest` 按 SYSTEM include；另修掉 3 处历史告警）。当前真实能力、示例索引与规格差距总览见手写文档 `docs/reference/capability_matrix.md`。两文档已对齐到该真值；历史数字（10/10、16/16、17/17、20/20、21/21、22/22、24/24）均为历史快照，不再引用。
- **R12 已收口（跨层缝，本轮）**：`src/canvas/scene_renderer.cpp` 原先把 `LineTo`/`ArcOp`/`RectOp` 轮廓画成 `kStrokeChar = U'*'`、`ImageOp` 填 `kMediumShade`。现在描边累积进 **braille 2×4** 子像素场、填充累积进 **halfblock 1×2** 子像素场，按当前 `StrokeOp`/`FillOp` 的颜色光栅化后叠加（只覆盖非空格，因此多份 Scene 可叠画同一 Canvas）；`Fill`/`Stroke` 的"作用于其后图元"语义与 `backend/vector` 的 SVG 后端正向同构。收口时抓到一个真 bug：**盲文的空白是 `U+2800` 而不是空格**，按空格判空会让每次收口用空白铺满整张画布、把先前画好的内容擦掉（实测折线被后续圆弧的收口抹去），已修并有 `canvas: 多次收口互不擦除` 回归测试。
  **同时澄清一处误判**：`chart`/`graph` 的示例（03/04/14/15/16/20）本就直连 `canvas.draw_line`/`draw_text` + 显式字符，**从未出占位符**（实测 `*` 计数为 0）；R12 的真实影响面是"同一份 `scene::Scene` 在终端侧能否出真图"，现在能了。
- 未落地：`dsl` / `widget` / `plugin` / `app`（L17–L20，零代码）；`graph` 其余子目录（layout 的 `kamada_kawai`、route 的 `obstacle`、diagrams 的 `dag`/`dependency`）；`interaction` 的 `highlight`/`select`/`brush`/`legend_toggle`/`immediate`；阶段 3 收尾项（`latex_backend`、`png_screenshot`、`37_export_latex`）；`chart` 其余图种（`cartesian/` 15 个）与 `polar/`；示例 `18_uml_sequence`。**规格差距量化**（代码清点，详见 capability_matrix 第五节）：按 `结构.md` 具名文件表核对，`include/` 现有 184 头而规格尚缺约 152–154 个；`tests/fixtures/` 目录已建（政策说明 + 7 份待补清单）。**已落地但曾在此清单里**：`layout` 四项、`graph/diagrams/orgchart`、`graph/layout/radial`+`circular`、`graph/route/spline`+`arc`、`interaction/zoom`+`pan`+`tooltip`+`crosshair`、`chart/cartesian/hist`+`heatmap`、`tests/fuzz/` 6 个目标、`example 17_uml_class`+`22_zoom_pan`。
- 权威来源分工不变：`结构.md` 回答"做成什么样"，本文件回答"什么时候做、做到哪"，`CODEBUDDY.md` 是给 AI 的架构摘要。

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

### 阶段 0 — 地基与护栏（最高优先级）

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
- [x] `util/`：`utf8` `grapheme` `wcwidth` `text` `bits` `math` `assert` `logging` `format` `geom`（点线距离/线段关系/多边形/凸包）`timer`（单调时钟/秒表/帧计时/超时）`scheduler`（单线程定时任务，注入时间源）`task`（可取消句柄）`base64`（**规格清单外**的补充：OSC 52 剪贴板与 kitty/iterm2 协议共用，term 不能反向依赖 protocol，故唯一实现下沉到 L1）
- [x] `color/`：`palette` `quantize` `named_colors` `gradient` `colormap`（viridis/inferno/magma/plasma 用公开多项式拟合；**turbo 改用官方 256 级色表**，见 `assets/colormaps/turbo.dat` → `tools/gen_turbo_table.py` → `detail/turbo_table.hpp`）`resolve`（Color/Style → 真正 RGB：None 走默认色、按 ColorDepth 先量化再取色，**GUI 与位图输出的分界点**）
  > Turbo 的历史实现踩过两次坑：先是多项式系数坏掉（输出恒为蓝/青/紫），改成锚点插值后又只是"形状像"。Turbo 本身没有解析式定义，Google 发布的就是一张 256 级查找表——所以现在的口径是"**只查表，不近似**"，级点处与官方表逐字节一致（采样时的浮点吸附见 `turbo_sample` 注释）。
- [x] `term/`：`terminal` `caps` `raw_mode` `size` `ansi` `daemon_safe` `signal` `waker` `mouse_mode`（模式与坐标编码两组独立开关 + 切换前清场）`clipboard`（OSC 52 读写与回包解析）`hyperlink`（OSC 8，参数位与 URI 位分别转义）`capability_query`（DA1/DA2 序列与回复解析）`color_fallback`（TerminalCaps → ColorDepth 桥接，算法唯一实现在 color/quantize）
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
- [x] `scene/`：`scene` `command` `builder` `visitor`（矢量 IR 本体；`command.hpp` 是 header-only）+ `bounds`（几何包围盒；GUI 侧 `scene_renderer` 要先知道 Scene 多大才能定画布/无效区，故放在 scene 而非 canvas）
- [x] `canvas/`：`canvas` `primitives` `scene_renderer` + **`fill`（扫描线填充，横向精确覆盖 + 纵向超采样）**、**`path`（曲线自适应细分展平，四段贝塞尔圆）**、**`layer`（Overlay/Replace/Tint 三种合成 + 最小重绘包围盒）**、**`transform`（2D 仿射，含字符宽高比补偿）**、**`clip`（矩形 + 凸多边形；非凸只支持逐点判定并在接口上报 false）**
- [x] `scale/` 起步：`scale` `linear` `log` `ticks` `nice` `auto_domain`（纯函数部分；`category` / `band` / `registry` 待补）
- [x] `image/bitmap.hpp`（`Bitmap`：RGBA 像素缓冲，宽/高/通道/行步长）+ `image/field.hpp`（`FloatField`：标量强度场，`raster/` 的输入）；两者都是 header-only。拆开是因为"图片像素"与"标量场"是两种东西，不该挤在一个 `bitmap.hpp` 里
- [x] `examples/02_sine_wave.cpp`
- [x] `tests/unit/raster/*` `image/*` `canvas/*` `scene/*` `scale/*`
- [x] `layout/`：`box` `axis` `grid` `legend` `title` `label`（在 canvas 之上做轴/网格/图例/标题，依赖 scale，不碰 render）
- [x] `chart/`：`chart` `series` `registry` `cartesian/line_chart` `cartesian/bar_chart`（端到端折线/柱状图，不碰 render）
- [x] `examples/03_line_chart.cpp` `04_bar_chart.cpp`
- [x] `tests/unit/layout/*` `chart/*`
- [ ] `tests/fuzz/fuzz_utf8.cpp` `fuzz_grapheme.cpp`
- [x] `bench/` 骨架建立：`bench/bench.hpp`（零依赖计时骨架，中位数取样 + `--quick` 冒烟档）、`bench/CMakeLists.txt`（`TERMCHART_BUILD_BENCH` 开关，`--quick` 进 ctest）；本阶段三条（`bench_raster` `bench_utf8` `bench_scene`）待补

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

### 阶段 3 — v0.3 图表与导出（架构契约验证点）

**目标**：三条渲染路径、三物种后端、注册表快照语义在此第一次交汇。**这是风险最高的阶段。**

#### 任务清单

- [x] `layout/`：`layout` `box` `axis` `grid` `legend` `title` `label` + `facet`（纯几何矩形划分，不认识 chart）`auto_margin`（由标签宽度/标题/图例反推留白）`label_avoid`（通用避让；graph 侧转引同一实现，规则只有一份）`annotation`（参考线/区域高亮/标注）。**规格 11 个文件全部落地**
  > `annotation` 有两条硬规则：**域外的值不画**（比例尺默认 clamp，硬画会得到一条"看着像真值"的边界线）；**域内的值要夹进 `[inner.y, inner.bottom()-1]`**（比例尺把域端点映射到半开边界上，不夹会整条消失）。区域高亮默认只改背景色、保留数据字符。
- [x] `chart/` 根：`chart` `registry` `series` `hit_region` + `context`（绘制开关与样式覆盖，默认全开）`dataset`（系列集合 + 范围查询）
- [x] `chart/cartesian/`：`line_chart` `bar_chart` `scatter_chart`（含气泡四档标记）+ **`area`（面积图：逐列填充到基线、baseline 纳入定义域、先填后描线）** + **`step`（阶梯图：`Pre`/`Post`/`Mid` 三档）** + **`hist`（直方图：Sturges 自动分箱、`set_bins` 可覆盖并 clamp 到 1..64、`bin_edges`/`bin_counts` 对外可查）** + **`heatmap`（热力图：二维点集→网格计数→`colormap` 真上色，色表可选）** + `sparkline`（迷你线，非 Chart 子类）。规格其余图种（15 个）待补；`area` 的堆叠/百分比/流图未做
- [x] `document/`：`document` `block` `span` `table_model`（structured 物种的输入 IR）
- [x] `backend/` 起步：根 `backend` + `snapshot/`（`backend` `ansi_backend` `text_backend` `html_pre_backend` `json_backend`，`ansi_backend` 复用唯一收窄边 `> render/sgr`）+ `vector/`（`backend` `svg_entities` `svg_backend`）+ `structured/`（`backend` `html_backend` `markdown_backend`）。三物种各至少一个后端；`png_screenshot` 待补
- [x] `tests/unit/document/*` `backend/*`
- [x] `interaction/`：`event` `key` `mouse` `input` `input_parser`（ASCII/UTF-8/CSI/SS3/SGR 鼠标/OSC/Alt/Ctrl，含跨包不完整序列与 **256 KiB 缓冲上限**）`focus` `hit_test`（只碰 `HitRegion` 几何，**无图表名**）`dispatcher` + **`zoom`（`Viewport`：锚点缩放 + 跨度/原点双向夹取，夹取时贴边优先）**、**`pan`（`PanGesture`：纯几何拖拽状态机，按"内容跟随指针"折算）**、**`crosshair`（准线 + 轴侧读数；默认只改样式保留数据字符）**、**`tooltip`（边框浮层 + 自动翻边 + 宽字符 2 列 + 超长截断）**
  > `crosshair`/`tooltip` 的唯一核心不变量是 **`clear()` 逐格逐属性精确还原**：覆盖层记下被改写的 `(格, 样式)`，撤销时放回去 —— 否则"悬停一次"就永久污染画布。两者都只在绘图区内绘制、退化输入（空文本/锚点在画布外/区域仅 1 格）**明确不画**而不是硬塞。
- [x] `scale/registry.hpp`（`ScaleRegistry`：function-local static 单例、工厂登记、内置 `linear`/`log`、`names()` 去重有序；与 chart/raster 同一套契约）
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

- [x] `graph/` 根：`graph`（节点/有向边容器 + 邻接/度数/并集包围盒）`node` `edge` `attributes`（字符串键值袋）`wiregrid`（原子占用，避免压线）`arrow`（头部字符与占格）`port`（锚点/对侧/朝向）`label_place`（右/左/上/下避让）`crossing`（真交叉计数）
- [x] `graph/layout/`：`layout`（选项 + 节点尺寸）`grid`（网格）`layered`（分层，含"整图成环时按入度最小者破环、回边不抬层号"）`tree_layout`（根树，支持左右分列）`force`（力导向，确定性圆环初始 + 降温收敛）+ **`radial`（径向树：父按子树扇区均分、半径按层递增、极坐标转格带字符宽高比压缩）**、**`circular`（圆周：按外接圆判据推半径，保证相邻框不重叠）**。`sugiyama`（本仓用 `layered` 覆盖其分层部分）`kamada_kawai` 待补
- [x] `graph/route/`：`route`（同侧端口均匀扇出 + 锚点吸附整数格 + 节点框登记为障碍）`straight` `orthogonal`（Z 形 → L 形 → 直线兜底，方向按节点框相对位置判定）+ **`spline`（Catmull-Rom + 弦长自适应采样 + 逐格去重 + 8 连通补齐；压框即回退正交——宁可折线也不穿框）**、**`arc`（三次贝塞尔隆起，控制点沿弦外推保证先离框；自环走框外 C 形环，折角二次贝塞尔抹圆）**
  > **交汇口字符已抽成公共件** `graph/route/junction.hpp`（header-only `constexpr`，登记在 `cmake/header_only.txt`）：规则原先在 `orgchart.cpp` 与 `17_uml_class` 示例里各有一份，地铁线路图会是第三个消费者，故在第三个出现前收口。它是"按上/下/左/右四个方向选 `──│┌┐└┘├┤┬┴┼`"的纯数据表判据，并有表驱动测试穷举 16 种组合 + 拓扑一致性检查（防 `┌/┐`、`├/┤` 写反）。
  > 两条新路由的真实签名是 `std::size_t route_spline(Graph&, const RouteOptions& = {}, WireGrid* = nullptr)`（`arc` 同）—— 与既有 `route_edges` 一致，返回写入折线的边数。`grid == nullptr` 时**自建**障碍栅格：不穿框是硬约定，不能因为调用方没给栅格就放弃避让（与 `route_edges` 行为不同，已在头注释写明）。`spline` **不处理自环**（自环两侧锚点重合会退化），自环请用 `arc`。`obstacle` 路由待补
- [x] `graph/shape/`：`shape`（轮廓 + 绘制，单一 `build_shape` 分派，绘制与轮廓不再各写一份 switch）`box` `rounded` `diamond` `ellipse`（逐行半宽取椭圆方程）`stadium` `hexagon`（六顶点 + 格粒度连线）`cylinder`（顶盖/侧边/底弧）`icon`（无边框，内容由调用方提供）；放不下时优雅退化为矩形/圆角
- [x] `graph/diagrams/`：`flowchart` `state_machine`（转移事件标签 + 起止标记，标签放不下时**放弃绘制**而不是涂掉节点）`mindmap`（树布局 + 扇形连线）**`orgchart`（层级树 + 共享树干 + `├┤┬┴` 接点收口）**`node_link`（通用节点-连线，供力导向图复用）。`dag` `dependency` 待补
  > `orgchart` 的观感靠"渲染出来看一眼"才定住：树干列若取 `child.x - 1` 会与子框边框糊成 `├│`；树干跨度用 `std::pair` 默认值当"空"会把 `0.0` 混进跨度、接点字符全算错。两处都已修，并有不变量测试钉住（树干不在父框边框列、不贴子框、两接点同列）。
- [x] `examples/14_flowchart.cpp` `15_state_machine.cpp` `16_mindmap.cpp` `17_uml_class.cpp` `19_metro_map.cpp` `20_force_graph.cpp` `21_hover_tooltip.cpp` `22_zoom_pan.cpp`
- [x] `tests/unit/graph/test_graph.cpp` `test_wiregrid.cpp` `test_diagrams_flow.cpp` `test_diagrams_orgchart.cpp` `test_graph_layout*.cpp` `test_graph_route.cpp` `test_graph_render.cpp`
- [x] `bench/bench_sugiyama.cpp` `bench_force.cpp`（已实测：`layout_layered` 64→1024 节点 120→2210 µs（≈线性）；`layout_force` 20→160 节点 / 200 迭代 2.6→146 ms（≈平方，**已知性能边界**，大图需 Barnes-Hut 或减迭代））

#### 出口条件

- [x] example 14·15·16·17·20 跑通（`17_uml_class` 已补：三栏类框 + `◁` 泛化箭头 + 总线接点）
- [x] sugiyama / force 有性能基线（`bench/bench_sugiyama` + `bench/bench_force`，数值与结论见 `docs/reference/capability_matrix.md` 第四节）
- [ ] `interaction > graph` 放行验证（分层规则已放行；待用一张图的实际命中判定补上端到端验证）

---

### 阶段 5 — v0.5 DSL 与交互

#### 任务清单

- [ ] `dsl/` 全套：`parser_base` `lexer` `ast` `mermaid` `mermaid_flow` `mermaid_sequence` `mermaid_class` `mermaid_state` `graphviz_dot` `plantuml` `d2` `table_markdown` `csv_sniff` `dsl_registry`
  > 已落地（2026-10-12，**14/14 全部**）：以上 14 个文件齐备。图产出方言经 `dsl_registry` 登记 6 个（flowchart / class / state / dot / plantuml / d2），`mermaid_sequence` 与表格类（`table_markdown` / `csv_sniff`）因产物不是图模型而**刻意不登记**（各有独立类型：`AstSequence`、`document::TableModel`）。
  > 并行分工：`mermaid_*` 三方言与 lexer/parser_base/ast/registry 由主线会话完成；`graphviz_dot` + `plantuml`、`table_markdown` + `csv_sniff` 由两个并行工作包完成，接口契约由 `ast.hpp` / `parser_base.hpp` / `lexer.hpp` 冻结，两包只新增自己的文件（收口时才动 `dsl_registry.cpp` 与 `tests/CMakeLists.txt`）。
- [ ] `interaction/` 深化：`tooltip` `crosshair` `highlight` `zoom` `pan` `select` `brush` `legend_toggle` `immediate`
  > 九件全部落地（2026-10-12 收口）：`tooltip` `crosshair` `zoom` `pan` 此前已有；本轮补齐 `highlight`（高亮放大）`select`（框选）`brush`（刷选）`legend_toggle`（图例切换）`immediate`（即时模式绑定），各配单测。
- [x] `examples/21_hover_tooltip.cpp` `22_zoom_pan.cpp` `30_legend_toggle.cpp` `33_mermaid_flowchart.cpp`
  > 四个示例全部存在且进 ctest（21/22 已有，30/33 本轮新增）。
- [x] `tests/unit/dsl/*` `interaction/*`
  > dsl：6 个测试文件（mermaid/dot/plantuml/csv/fixtures/d2）；interaction：新增 legend/highlight/select/brush/immediate 五个测试文件。
- [x] `tests/fuzz/fuzz_mermaid.cpp` `fuzz_dsl_lexer.cpp` `fuzz_input_parser.cpp`
  > `fuzz_mermaid` 与 `fuzz_dsl_lexer` 已建（`--self-test` 进 ctest，8 个 fuzz 目标全绿）；`fuzz_input_parser` 早已存在。
- [x] `bench/bench_mermaid_parse.cpp`
  > 已建：四方言（mermaid/dot/plantuml/d2）× 200/400/800/1600 节点基线，`--quick` 进 ctest。实测 200 节点：mermaid 2.2ms / dot 4.1ms / plantuml 6.2ms / d2 3.8ms；规模翻倍耗时约 2.3 倍（线性，无平方级回归）。
- [x] `tests/fixtures/sample_mermaid.txt` `sample_dot.txt` `sample_plantuml.txt` `sample.csv`
  > 四份夹具全部由生成器 `tools/gen_dsl_samples.py` 产出（已接入 `generate_all` / `check_generated`），并由 `tests/unit/dsl/test_dsl_fixtures.cpp` 断言"原样可解析且覆盖受支持子集"。

#### 出口条件

- [x] example 21·22·30·33 跑通（全部在 ctest：42/42，2026-10-12）
- [x] 三个 dsl 相关 fuzz 目标可运行（`fuzz_mermaid` / `fuzz_dsl_lexer` / `fuzz_input_parser`，`--self-test` 各 24 条语料）
- [x] `dsl_registry` 遵循 function-local static + 快照语义（单测锁死：定例同一、重名拒绝、快照独立副本、幂等内置登记）

---

### 阶段 6 — v0.6 图像·协议·组件·应用

#### 任务清单

- [x] `image/`：`bitmap`（RGBA + `fill_rect`/`blit_over`/`scaled_to`/通道转换等合成原语）+ `field`（标量场）+ `convert`（位图 ↔ 标量场）+ **`png_encode`**（自包含编码：CRC-32 / Adler-32 / stored deflate 块）+ **`png`（解码）**（自带完整 inflate：stored/固定/动态 Huffman、五种滤波、颜色类型 0/2/3/4/6、1/2/4/8/16 位深、tRNS；不支持 Adam7，16 位取高字节）+ **`decoder`**（按魔数识别 PNG/JPEG/GIF/BMP/WebP 并分发）。**规格 4 个文件全部落地**
  > 交叉验证：编码器输出用 Python 的 zlib/binascii 校过；解码器用 `tools/gen_png_samples.py` 生成的 12 个夹具样本（滤波 1–4、动态 Huffman、调色板 4/8 位、灰度 16 位、灰度+alpha、真彩+alpha、坏 CRC、Adam7）覆盖——只靠自家编码器往返测不到这些分支。
  > `bitmap` 的合成原语与 `convert` 是规格清单外的补充：GUI 侧要缩放/叠加/上传纹理，终端侧的 sixel 缩略与 PNG 拼版同样需要；`convert` 是"图片 → 盲文/密度预览"的唯一实现（Rec.601 亮度，alpha 乘进结果，透明像素在场里是 0 而不是黑斑）。
  > 取舍：`png_encode` 不引入 zlib，用 stored（未压缩）deflate 块换取"零依赖 + 完全合规"，体积约 +0.03%；将来接真 deflate 只换内部实现。已用 Python 参考 zlib 独立校验：IDAT 解压字节数、滤波字节、逐 chunk CRC、`size=` 声明全部吻合。
- [x] `protocol/`：`protocol`（基类）`sixel`（量化+调色板+RLE）`kitty_graphics`（原始 RGBA+base64 分块）**`iterm2_inline`**（PNG + base64 + OSC 1337）`halfblock_image`（真彩半块）`fallback`（Kitty > Iterm2 > Sixel > HalfBlock > None）`detect`（环境探测，含 WezTerm/Konsole/iTerm 与手动覆盖开关）
- [ ] `widget/`：`widget` `container` `vbox` `hbox` `grid_layout` `dock` `panel` `border` `scroll` `styles` `style_sheet` `theme` `theme_dark` `theme_light` `theme_mono` `immediate_widget` `reactive`
- [ ] `app/`：`app` `loop` `animation` `config` `post` `immediate_app` `headless`
- [ ] `plugin/`：`plugin` `loader` `dynamic` `manifest`
- [ ] `api/` 收尾：`quick_line` `quick_bar` `quick_spark` `quick_table` `termchart_full.hpp`
- [ ] `examples/24_theme_switching.cpp` `25_animation.cpp` `26_realtime_stream.cpp` `27_background_thread.cpp` `39_headless_render.cpp` `40_immediate_mode.cpp` `41_reactive_dashboard.cpp`
- [x] `examples/38_image_sixel.cpp`（Bitmap → 探测 → 选协议 → 写 stdout）
- [x] `tests/unit/protocol/*` `image/*`；`widget/*` `test_plugin.cpp` `test_capability.cpp` 待补
- [ ] `tests/integration/test_app_loop.cpp` `test_thread_post.cpp` `test_resize.cpp`
- [x] `tests/fuzz/` 建立：`fuzz_common.hpp`（三档运行：`--self-test` 内置语料 / `--iterations N` 确定性随机 / `<file>` 复现；内置 xorshift64* 与 15 条边界语料）+ **6 个目标** `fuzz_utf8` `fuzz_grapheme` `fuzz_input_parser` `fuzz_colormap` `fuzz_sixel` `fuzz_png`（末者为规格外补充：`decode_png` 是全仓最理想的 fuzz 入口）
  > 规格 7 个目标里的 `fuzz_mermaid` / `fuzz_dsl_lexer` **刻意不建空壳**：L17 `dsl` 是空层，没有可调用的解析器，建了只能空转。`--self-test` 已接进 `ctest --preset fuzz`（13 项），并以专用 preset `fuzz` 隔离（默认构建零影响）。

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
- [ ] `LICENSES/` + `assets/`（**`assets/colormaps/turbo.dat` 已落地**；maps、logo、banner 待补）
- [ ] 根目录文档：**`README.md` 已落地**（定位、当前状态真值、快速开始、构建与护栏、架构一眼看、目录、许可）；`README_zh` `CHANGELOG` `CONTRIBUTING` `CODE_OF_CONDUCT` `AUTHORS` `ABI_POLICY` `ERROR_POLICY` `SECURITY` `AGENTS` 待补

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
| R12 | 高级路径占位渲染（Scene→raster 断层）— **已收口** | 原 `canvas/scene_renderer.cpp` 用占位字符 `kStrokeChar=U'*'` 画所有线/弧/矩形轮廓、`ImageOp` 用 `kMediumShade` 填充。现已接 raster：描边累积进 **braille 2×4** 子像素场、填充累积进 **halfblock 1×2** 子像素场，按当前 `StrokeOp`/`FillOp` 颜色光栅化后叠加（仅覆盖非空格，多份 Scene 可叠画同一 Canvas）；`Fill`/`Stroke`「作用于其后图元」语义与 `backend/vector` 的 SVG 后端正向同构。收口时修了一个真 bug：盲文空白是 `U+2800` 而非空格，按空格判空会擦掉先前内容（回归测试 `canvas: 多次收口互不擦除`）。`raster`（盲文/半块/象限/块）本身已实现且可用 | 现状：同一份 `scene::Scene` 现已能在终端侧出真盲文/半块图，外观由 golden `scene_raster/basic.txt` 锁住。**唯一残留占位为 `ImageOp`**：因 `scene` 层按分层规则禁止依赖 `image`，Scene 里没有像素数据可光栅化（同一份 Scene 在 SVG 等 vector 后端正常出图）。`chart`/`graph` 示例（03/04/14/15/16/20）本就直连 `canvas.draw_line`/`draw_text`，从不经 Scene，故从未是占位符 | 0/7 |

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