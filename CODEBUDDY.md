# CODEBUDDY.md This file provides guidance to CodeBuddy when working with code in this repository.

> 仓库阶段：规格先行、分阶段落地。已实现 `core / util / color / term / render / api / scene / canvas / scale / image / raster / layout / chart / document / backend / protocol / interaction` + `detail/`；`scene/`（矢量 IR）与 `canvas/`（格粒度画布 + Scene→Canvas 桥接）已接通，`scale/`、`image/`（`Bitmap` RGBA + `FloatField` 标量场 + 自包含 `png_encode`）、`raster/`、`layout/`（轴/网格/图例/标题）、`chart/`（折线/柱状 + 注册表 + `hit_region`）、`document/`（Document IR）、`backend/`（**三物种**：snapshot ansi/text/html_pre/json、vector svg、structured html/markdown）、`protocol/`（sixel / kitty / iterm2 内联 / halfblock / fallback / detect）、`interaction/`（输入解析 + 命中测试 + 分发 + 焦点）、`graph/`（根：节点/边/元数据/连线栅格/箭头/端口/标签避让/交叉计数；子目录：`shape` 形状轮廓、`layout` 网格与分层布局、`route` 直线与正交路由、`diagrams/flowchart` 端到端渲染）已落地，`dsl/`、`widget/` 等 4 个模块（dsl/widget/plugin/app）与 `graph` 其余子目录（layout 的 sugiyama/force 等、route 的 spline/arc、shape 的其余形状、diagrams 的其余图种）尚在 `结构.md` 里。写任何代码前先确认目标阶段是否已开工；规格问题查 `结构.md`，进度问题查 `ROADMAP.md`，二者都不查本文件。
>
> 阶段 0（护栏）与阶段 1（终端上色闭环）已跑通，阶段 2 的 `Scene → Canvas → CellBuffer`、`scale/`、`image/`、`raster/`，阶段 3 起步的 `layout/`、`chart/`、`document/`、三条 `backend/`（snapshot/vector/structured），以及阶段 6 起步的 `image/`（Bitmap）/`protocol/` 已接通：库已能把数据系列画成带坐标轴的终端折线/柱状图（`examples/03`、`04`），把 `CellBuffer`/`Scene`/`Document` 分别导出成 ANSI·文本·HTML·JSON / SVG / HTML·Markdown（`examples/34`、`35`、`36`），并在终端里用 sixel/kitty/半块显示位图（`examples/38`）。**三个可扩展性契约实验**（新增 backend / chart / raster，核心零改动，`tests/integration/replace_*.cpp`）与 **text/svg/json 三类 golden** 已建立；`interaction/` 起步（ANSI/UTF-8/SGR 鼠标字节流解析、HitRegion 命中测试、事件分发、焦点链）已落地 —— 阶段 3 的交付项至此全部勾选。图可视化可从 `Graph` 一路画到终端：`examples/14_flowchart`（分层布局 + 正交路由 + 形状/标签/箭头）、`15_state_machine`（转移事件标签 + 起止标记）、`16_mindmap`（树布局 + 扇形连线）、`20_force_graph`（力导向 + 节点-连线视图）。`Scene → Canvas` 也出真图（`render_scene` 描边走 braille 2×4 子像素、填充走 halfblock 1×2 实心，R12 已收口，golden `scene_raster/basic.txt` 锁外观）。性能基线已建立（`bench/bench_sugiyama`、`bench/bench_force`，零外部依赖、`--quick` 进 ctest 冒烟）。`ctest` 24/24、doctest 335 用例 / 185558 断言全绿、`check_all`（分层 + 自洽 + 生成物 + **每头独立编译**，141 头 / 107 实现）通过。**当前真实能力与规格差距以手写文档 `docs/reference/capability_matrix.md` 为准**（含示例索引：哪些示例走真 raster / 哪些走 Scene）。

## 常用命令

**配置 + 构建（首选）**：`cmake --preset ninja` 后 `cmake --build --preset ninja`，测试 `ctest --preset ninja`。`ninja` / `ninja-release` 是跨平台预设（Ninja 生成器），在 Windows 上比 `*-windows` 预设可靠。sanitizer 与覆盖率预设（asan/tsan/coverage）只在非 Windows 预设里。构建目录是 `build/<presetName>`。 (70字)

**Windows 备用预设**：`cmake --preset debug-windows`（MinGW Makefiles）。注意 GNU make 在含非 ASCII 字符的路径下会**把参数按 ANSI 破坏**，`mingw32-make` 连编译器自检都过不了；工作区路径含中文时必须用 `ninja` 预设。 (60字)

**跑测试**：`ctest --preset ninja`。`tests/CMakeLists.txt` 注册了 5 个套件名（`termchart_tests` 全量 + `tests_golden` / `tests_render` / `tests_core` / `tests_util` 过滤跑），外加每个 example 一个 `example_<name>` 测试（当前共 16 个）。 (60字)

**跑单个用例（最快）**：直接跑 doctest 二进制并过滤，绕开 ctest：`build/debug-windows/tests/termchart_tests --test-case=*core*`，或 `--source-file=` / `-tc=` 精确定位。源文件→用例的对应关系在 `tests/CMakeLists.txt` 里一目了然。 (55字)

**护栏三件套**：`cmake --build --preset <预设> --target check_all`（依赖 `check_layers`、`check_header_only`、`check_generated`）。等价的直接调用：`python tools/check_layers.py`、`python tools/check_header_only.py`、`python tools/gen_{unicode_tables,grapheme_tables,braille_table}.py --check`。改了 `#include` 必跑分层校验。 (70字)

**重新生成产物**：`cmake --build --preset <预设> --target generate_all`，或直接 `python tools/gen_unicode_tables.py`、`gen_grapheme_tables.py`、`gen_braille_table.py`、`gen_module_graph.py`。`--check` 非零即表示生成物与生成器不一致，**只能重跑生成器，不得手改产物**。 (60字)

**最小构建**：`cmake -DTERMCHART_MINIMAL_BUILD=ON`，只保留 core/util/color/term/render/api（开关在 `cmake/TermchartOptions.cmake`）。可选依赖 libpng/zlib/utf8proc 默认 OFF，缺失只降级不失败。 (40字)

**更新 golden 快照**：`python tools/gen_golden.py`（自动在 `build/*/tests/` 找测试二进制，也可把二进制路径作第一个参数），或 `cmake --build --preset ninja --target generate_golden`。原理是置 `TERMCHART_UPDATE_GOLDEN=1` 后跑 golden 用例，由 `golden_runner.cpp` 自己写文件；禁止手改快照。 (70字)

**每头独立编译**：`cmake --build --preset ninja --target check_headers`（或 `python tools/check_headers.py --compiler <CXX>`）。为 `include/termchart/**` 每个头生成一个只含它的 TU 并 `-fsyntax-only` 编译，任一失败即退出码 1。新增/修改头文件后必跑——GUI 平行栈会把这些头单独 include 进自己的 widget。 (80字)

**本机环境注意**：`cmake` / `ctest` 不在 PATH 上（pip 装的 cmake 位于 `%APPDATA%\Roaming\Python\Python314\site-packages\cmake\data\bin\`），需写全路径；`ninja` 在 `%APPDATA%\Roaming\Python\Python313\Scripts\ninja.exe`（已在 PATH）。编译器是 `C:\Program Files\mingw64\bin\c++.exe`（GCC 15.2）。 (70字)

## 架构总览

### 定位与规格层级

termchart 是零强制依赖的 C++20 终端图表库。`结构.md` 定义 22 个模块、约 400 个头文件的目标形态；`ROADMAP.md` 定义 v0.1–v0.7 八个阶段的垂直切片与「出口条件」，并声明自己是**唯一进度口径**——判断「某功能是否该有」只以它的勾选清单为准。当前代码处于阶段 0（地基护栏）+ 阶段 1（终端上色闭环）：`include/termchart/` 下只有 core、util、color、term、render、api 六个模块目录、横切的 `detail/`，以及 `version.hpp / export.hpp / deprecate.hpp / fwd.hpp / termchart.hpp` 五个顶层头。

### 分层规则：机器强制，唯一真源

模块 = `include/termchart/` 下的一级目录，被赋予层号 L0–L21：`core(0) → util(1) → color(2) → scale(3) → scene(4) → image(5) → raster(6) → term(7) → document(8) → protocol(9) → canvas(10) → layout(11) → chart(12) → graph(13) → render(14) → interaction(15) → backend(16) → dsl(17) → widget(18) → plugin(19) → app(20) → api(21)`。

规则**只**写在 `cmake/TermchartModules.cmake`（`TERMCHART_MODULE_LAYERS` / `TERMCHART_NARROW_EDGES` / `TERMCHART_FORBIDDEN_EDGES`），由 `tools/check_layers.py` 执行，判定顺序固定为 **禁止边 → 收窄边 → 层号**。三个例外：`detail/` 层号 -1（任何层都可 include）；顶层 `version/export/deprecate/fwd` 视为 core（层号 0）；两个伞形头豁免层号判定。

三条登记边承载了关键设计意图，改动前必须先理解：

- `backend > chart`（禁止）：backend 只走 Document IR，绝不碰 chart 类型。
- `chart > render`（禁止）：chart 不碰渲染管线，保住未来的 `to_document()` 通路。
- `render > interaction`（禁止）：刻意维持单向，`interaction` 可以 include `render`。
- 收窄边 `backend/snapshot/ansi_backend > render/sgr`：导出路径要复用唯一的 SGR 原语，于是把 `backend > render` 收成文件级白名单，两个模块内其余文件互相全部关闭。

注意层号与直觉不同：`term` 是 L7 而 `render` 是 L14，所以 `render` 可以依赖 `term`，反过来不行；`api` 在顶层 L21，是唯一能碰所有模块的地方。

### CellBuffer 是中枢，两条下游路径分野

`core::CellBuffer`（`core/buffer.hpp`）是全库中枢：连续 `vector<Cell>` 网格 + 稀疏侧面表（`unordered_map<index, array<char32_t, N>>`，绝大多数单元没有组合记号，逐单元存数组会让体积翻倍），对外暴露 `std::span<Cell>`（调用方不得保存跨越 `resize` 的 span）。它持有堆资源，**禁止进 `core/arena.hpp` 的帧分配器**。

`core/cell.hpp` 只存**单个主码点** `char32_t`；组合序列结构在 `core/grapheme.hpp`（只定义，不分割），**真正的 grapheme 分割在 `util/grapheme.hpp`**。`detail/grapheme_tables.hpp`、`detail/unicode_tables.hpp` 是生成的区间表，约定「升序且互不重叠」，被 `tests/unit/test_layering.cpp` 作为结构不变量守住——手改生成物会在那里先失败一次。生成器的 `build_ranges()` 也会自校验：区间一旦重叠直接 `SystemExit`，二分查找的正确性依赖它。

**宽字符占两格，第二格是续接格**：`Cell` 里 `cp == kContinuationCp`（`U'\0'`）表示该格是宽字符的右半边，由生产者（`api/quick.cpp` 写 `Cell(kContinuationCp, style)`）负责填，`render/renderer.cpp` 的 `emit_cell_text` 负责**跳过它、不写任何字节**。漏掉这个约定，渲染器会在宽字形右半边补一个空格把字形覆盖掉——`tests/golden/ansi_props/wide_chars.txt`（4 格 / 8 列）就是这条约定的回归测试。

渲染路径有三条，写后端前必须先确定输入是哪一种：

1. **终端实时**：`CellBuffer` → `render/renderer.hpp`（持有 prev 状态 + `render/diff.hpp` + `render/damage.hpp`）→ 增量字节流 → 由 `api/print.hpp` 或调用方写 stdout。`Renderer` 只生成字节不写 stdout，因此在 CI/headless/单测里完全可用。
2. **snapshot 导出**（阶段 3）：`CellBuffer` → `backend/snapshot/*`，一次性、无 diff、无 prev。
3. **vector 导出**：`Scene` → `backend/vector/*`；**structured 导出**：`Document` → `backend/structured/*`。

`render/sgr.hpp` 是**唯一 SGR 序列生成点**：实时路径与 snapshot 后端都用它。它同时提供 `append_style`（完整）与 `append_style_delta`（只发变化的那部分），后者决定每帧输出量。任何地方都不许再拼一套 SGR。

### 错误处理与符号边界

不抛异常、`std::expected` 是 C++23 设施，因此 `core/expected.hpp` 自带 `Expected<T,E>`（基于 variant，语义对齐 `std::expected`），并派生 `Result<T> = Expected<T, ErrorCode>` 与 `Status = ErrorCode`。约定：只回报成败的函数返回 `Status`，需要携带值的才用 `Result<T>`；无值访问 `value()` 是调用方 bug，抛异常（实现在 `src/core/expected.cpp`，避免头文件膨胀）。

对外接口普遍用 PIMPL（`render::Renderer` 持有 `Impl*`），`ABI_POLICY.md` 只承诺 PIMPL 门面稳定。符号可见性统一走 `export.hpp` 的 `TERMCHART_API`；静态构建时 `TERMCHART_STATIC` 是 PUBLIC 定义。

### 平台隔离

`term/terminal.hpp` 是终端状态的唯一持有者：构造不动任何东西，`enter()` 按 `TerminalOptions` 逐项启用并**记录已生效项**，析构（含异常栈展开）按相反顺序精确回滚；`term/daemon_safe.hpp` 负责非 tty 退化。该文件**没有任何 `#ifdef`**——平台差异全部落在 `src/term/terminal_posix.cpp`、`terminal_win32.cpp`、`waker_win32.cpp` 与私有头 `src/term/platform_internal.hpp`（它以 `src/...` 路径被 include，顶层 `CMakeLists.txt` 为此把仓库根加进了 PRIVATE include 目录）。新增平台分支必须走这层，不许在公开头里加 `#ifdef`，否则算架构泄漏。`tests/integration/test_terminal_restore.cpp` 守这条。

### 物理布局与生成物

`include/termchart/**` 与 `src/**` **严格同路径同名**，一个头一个 cpp（基类也要有 cpp 放虚表锚点），例外只有 `cmake/header_only.txt` 列出的纯头（当前含 `core/point|rect|attr|color_depth|rgb|span_utils`、`util/bits|math|random|assert`、`color/named_colors`）。新增头文件要同步做全套：CMake 引用、`src/` 同名实现、单元测试、自洽性校验。

生成物禁手改：`include/termchart/detail/*.hpp` 由三个 `tools/gen_*.py` 产出；`结构.md` 附一/附四与 `docs/reference/module_matrix.md` 由 `tools/gen_module_graph.py` 从 `TermchartModules.cmake` 产出；`single_include/` 与 `tests/golden/` 同样是生成物。

### 测试形态

doctest 是 vendored 的唯一框架（`third_party/doctest/doctest.h`）。`tests/unit/` 按模块分组；`tests/unit/test_layering.cpp` 守生成数据表的结构不变量而非模块分层（模块分层由 `check_layers.py` 守）。golden 当前只有 `ansi_props` 一类，且刻意**把渲染字节流解析回属性（行/列/码点/前景/背景/属性位）再比对**，不比字节——字节级快照会把「先发 fg 还是先发 bg」这类无关实现选择固化成契约，只产生噪声。`tests/integration/test_full_render.cpp` 覆盖端到端链路。

### 改代码时最容易踩的坑

1. 手改 `结构.md` 附一/附四、`cmake/header_only.txt`、`detail/*.hpp`、`tests/golden/` —— 全是生成物。
2. 在 `chart/` 里 include `render/`，或在 `backend/*` 里依赖具体 chart 类型 —— 两条禁止边。
3. 给 snapshot 后端重写 SGR —— 统一用 `render/sgr.hpp`。
4. 在公开头里加平台 `#ifdef` —— 走 `src/term/*_posix.cpp` / `_win32.cpp`。
5. 在 `App` 运行期注册插件或 scale —— 注册的唯一安全时机是 `App` 构造前（注册表模型见 `docs/design/registry.md`，`instance()` 一律 function-local static，App 构造时冻结浅拷贝快照）。
6. 新增头文件忘记同步 `src/` 同名 cpp 或单元测试 —— 自洽性校验会拦。
7. 忘了宽字符的续接格（见上文）：写 buffer 的生产者必须自己填 `kContinuationCp`，渲染器不会替你推断。
8. 以为 `style_sequence()` 只发"设过的"参数：它是**完整序列**，未设的前/背景会发 `39` / `49`；要省字节走 `append_style_delta()`。
9. 在测试里写 `CHECK(a || b)`：doctest 的表达式分解不接受，必须再包一层括号 `CHECK((a || b))`。
10. Windows 中文环境"乱码 + ANSI 裸露"通常不是库的 bug——程序已输出正确 UTF-8 与真彩色 SGR。两端要分别处理：库层由 `Terminal::enter()` 调 `platform::enable_vt()` 开 VT 让控制台解释转义序列；PowerShell 侧另需 `[Console]::OutputEncoding=[Text.Encoding]::UTF8`（`chcp 65001` 只改控制台代码页，管不到解码层）。最省事：用 Windows Terminal / VS Code 终端。
11. **绘图前必须用 `terminal.size()` 自适应**：把画布尺寸写死成大于终端可视区时，超宽会每行折行、超高会滚动，两者叠加使图像错位（像"颜色溢出"，甚至顶部被截断）。宽取 `cols`、高取 `rows - 1`（留一行给命令行提示符），并按字符格约 1:2 的宽高比等比取景避免变形。
12. 以为负坐标会折到画布边缘：`Canvas` 对负坐标是**裁剪**（`put`/`draw_text` 丢弃左/上边界外的格），极大/Inf/NaN 坐标被饱和或丢弃——**任何**落格路径都不会越界写。
13. 以为 `Scene` 的 `FillOp`/`StrokeOp` 作用于"之前的"几何：它们是**状态指令，只作用于其后出现的图元**（与 SVG 同构，见 `canvas/scene_renderer.cpp`）。SVG 等 vector 后端必须同构，否则 `几何 → Stroke` 会着色错位。
14. 正交路由的 Z 方向不能按"锚点距离"选：必须按**两个节点框的相对位置**（x 分离就横向优先）。按距离选会让绕行坐标跑到源框的边框列上（该列下方格全在框内，被障碍否决）甚至越出画布，最终退化成斜线。方向必须由调用方传给折线构造函数，函数内部不得再自行判断。
15. 节点框必须**先登记进 `WireGrid` 当障碍**再路由，否则折线会穿过别的节点；路径检查时首末格（落在节点边框上的锚点）要跳过。同侧多条边还要按 `(节点, 侧)` 分组均匀分配锚点，否则它们共用同一格，后路由者必然退化。
16. `Scene → Canvas` 的取字符：`canvas/scene_renderer.cpp` 自 R12 收口起，描边累积进 **braille 2×4** 子像素场、填充累积进 **halfblock 1×2** 子像素场，按当前 `StrokeOp`/`FillOp` 颜色光栅化后叠加（只覆盖非空格）。**注意盲文的空白是 `U+2800` 而不是空格**——判空时两者都要跳，否则每次收口会用空白铺满画布、擦掉先前内容（曾有真 bug，见 `canvas: 多次收口互不擦除`）。另：`chart`/`graph` 的示例本来就走 `canvas.draw_line`/`draw_text` + 显式字符，不经 Scene，因此它们从来不是占位符（实测 `*` 计数为 0）。

> **进度口径说明（2026-10-11 干净重跑确权）**：`ctest 24/24`（5 套件 + 17 example + 2 bench 冒烟）、doctest `335 用例 / 185558 断言` 全绿，由 `ctest --preset ninja` 权威确认；`check_all` 通过（141 头 / 107 实现）；golden 9 份（含 `scene_raster`）。规格差距量化见 `docs/reference/capability_matrix.md` 第五节（规格具名头文件尚缺约 196 个；`tests/fuzz/`、`tests/fixtures/` 仍未建立）。本文件与 `ROADMAP.md`「一、项目现状」已对齐到该真值。读进度优先看 ROADMAP「一、项目现状」的审计结论（含逐模块 LOC 与 Scene→raster 收口 R12）。
