# 还没做清单（Backlog）

> 本文件是 `ROADMAP.md` 的「还没做」总览子文档，索引见 ROADMAP 顶部 CHANGELOG（2026-10-13 一行）。
> 基于 **2026-10-13 `53680db`(phase7) 提交后的真实状态（git 实证）** + 权威口径（`ROADMAP.md` 现状节 / `docs/reference/capability_matrix.md` 第五节）。
> **重要**：`ROADMAP` 现状节与 `capability_matrix` 第五节是 2026-10-11 快照，phase7 已落地 `dsl`/`widget`/`plugin`/`app`/`diagrams`/`single_include`，故其中"未落地 / 缺口"数字**已失真**。本清单按真实状态重列，并把"文档待同步"单独列为第四节。
> 勾选规则同 `ROADMAP.md`：代码落地 + 测试通过 + 校验脚本通过，才打 `[x]`。

---

## 一、范围 / 规格缺口（功能层面）

- [x] **L18 widget / L19 plugin / L20 app 完整性**（2026-10-13 确权）：阶段 6 出口条件闭合——示例 24–27·40·41 已随 backlog 轮落地并进 ctest；并发测试见 `tests/unit/app/test_app.cpp`（post 跨线程先投递后消费）。
- [x] **graph 剩余布局算法**：`layout/kamada_kawai` 已落地（KK 能量布局 + 多分量并排 + 退化口径），测试 `test_graph_kamada_kawai.cpp` 全绿。
- [x] **graph/route/obstacle 横挡绕障**：缺陷已修并加回归测试 `test_graph_route_obstacle_fix.cpp`（绕行段不穿障碍）。
- [x] **导出格式补齐**（2026-10-13）：`latex_backend` + `png_screenshot` + `37_export_latex` 示例（片段/standalone 双模式）全部落地。
- [x] **EPS 矢量导出后端**（2026-10-28 新增）：`backend/vector/eps_backend` 把 `Scene` 翻成 EPSF-1.2
  （`%!PS-Adobe` 头 + `%%BoundingBox` + Helvetica 文本 + 真彩 RGB，弧以折线逼近），镜像
  `pdf_backend` 的 `StatefulVisitor` 遍历（PostScript 与 PDF 内容流算子同源，去掉 xref/对象表即可）。
  填补此前"仅 PNG/BMP 编码、无 EPS"的矢量导出缺口（PDF/SVG 已有）。4 个单测（`test_eps_backend.cpp`，
  头/尾/路径算子/填充色）全过；全量 1327 用例 0 失败。
- [x] **自包含 baseline JPEG 编码器**（2026-10-28）：`image/jpeg_encode` 零依赖实现
  FDCT + 标准量化表(按 quality 缩放) + 标准 Huffman（亮度/色度各 DC/AC）+ 之字序 + 0xFF 填充，
  输出合规 JFIF（SOI/APP0/DQT/SOF0/DHT/SOS/EOI）。`examples/83_encode_jpeg.cpp` 走真链路
  SceneBuilder→SceneRenderer→Bitmap→encode_jpeg；`test_jpeg_encode.cpp` 校验标记结构 + 纯色 Q100
  round-trip（(214,40,41) vs (214,39,40)）。修复 SOF0 段长少 2 字节、FDCT 的 DC 项 C(u) 重复归一化两处真 bug。
- [x] **自包含 VP8L lossless WebP 编码器**（2026-10-28）：`image/webp_encode` 零依赖实现 RIFF/WEBP/VP8L
  容器 + 5 棵 canonical Huffman（GREEN 280 / R·B·A 256 / DIST 40），码长经码长码发射、码字按位反转对齐
  libwebp/ffmpeg/PIL 解码；逐像素 G,R,B,A 字面量（无变换/无色彩缓存/无 LZ77，v1 最稳形态）。接进统一分发器
  `encode(ImageFormat::Webp)`；`examples/85_webp_lossless.cpp` + `test_webp_encode.cpp`（9 用例/91 断言，含
  独立 VP8L 解码器逐字节回环 + ffmpeg/PIL 落盘校验）。修复 meta-Huffman 标志位、RIFF 奇长度 pad 两处真坑。
- [x] **examples 补齐到规格**（2026-10-13）：规格表 01–54 全部落地并进 ctest（`ctest` 73/73），无缺口。
- [x] **具名头缺口重数**（2026-10-13）：`capability_matrix` 第五节已按实测 **333 头 / 282 实现** 刷新，缺口改为"约 26–28 个"。
- [x] **fuzz 目标**：`fuzz_mermaid` / `fuzz_dsl_lexer` 早已建好并进 ctest（backlog 该条基于过期快照）——实存 8 个 fuzz 目标全绿。
- [x] **tests/fixtures 7 份待补**（2026-10-13）：新增 `tools/gen_fixtures.py`，补齐
  `sample_data.hpp` / `sample_graph.hpp` / `sample_grapheme.txt` 三份（前 4 份仍由
  `gen_dsl_samples.py` 产出）；**7 份全部就位**，且都有消费方 `tests/unit/test_fixtures.cpp`。
- [x] **「数据帝国」三模块**（2026-10-21）：math(L1) / io(L2) / data(L4) 落地——
  math：stats / interpolate / fft / linalg / special 五头（详见 ROADMAP 修订记录）；
  io：csv 零拷贝数值入口、json（dsl/json 核心下沉收编，dsl 留 shim）、stream 分块行读取；
  data：util/data 四件套上移（git mv，namespace termchart::data）+ ema + ohlc。
  验证：doctest 1103 用例 / 194733 断言 0 失败、ctest 79/79、check_all 全绿。
- [x] **SymLog（对称对数）轴**（2026-10-28 收尾）：`scale` 此前只有线性与对数（对数轴拒非正值），
  没有 `symlog`。现 **`SymLogScale`（`linthresh`；`linscale` 暂未接，matplotlib 同款对称对数语义已够用）
  + 跨零点刻度生成**均已落地**——`ticks_for` 通过 `dynamic_cast` 对 `SymLogScale` 分发到专用
  `symlog_ticks`：线性区（[-C,C] 插 nice 细分 + 0 + ±C）与对数区（±C·10^k 及 2C/5C 中间档）拼接，
  单调升序、去重、防爆抽稀。这解决了原 backlog 标注的**主要风险点**（tick 序列与网格对齐）。
  新增 2 个单测（跨 0 含 0/±C/对数档；全在线性区内只给线性细分）。
  +  剩余可选：`layout` 的零线加重 / 负区阴影（目前刻度已含 0 与 ±C，轴线表达已可用，强调为锦上添花）。
    **决策说明保留**：原"明确不在近期做"的判据（量级多为脏数据、靠 Tukey 篱裁剪覆盖）仍成立；本收尾是
    把先前并行 Agent 已实现、却未被 backlog 勾销的 `SymLogScale` + 本次补全的刻度一并登记闭环，而非
    主动为单一图种推动 22 层架构变更。触发条件（量级本身是业务信号）仍未出现。
- [x] **chart → 真实 Scene 的图种迁移**（2026-10-28 收尾核实）：`Chart::to_scene` 通路已建
  （`layout::Sink` 双落笔端 + `chart::SceneSink` + `scene::GroupOp` + `api::chart_to_scene` /
  `chart_to_svg`，示例 `78_chart_scene`）。**原 backlog 登记的"只有 LineChart/BarChart"已过时**——
  实测 `src/chart/**` 下约 80 个图种 cpp **全部有真实 `to_scene` 实现**（抽查：`scatter` 画网格/轴/标记、
  `pie` 逐格扇区填充进 `SceneSink`、`area`/`hist`/`boxplot`/`heatmap` 等均为非空实现），且
  **测试已覆盖**：`tests/unit/chart/` 下 `test_chart_to_scene_batch1–5.cpp` + `test_chart_scene.cpp` +
  `test_scene_animation.cpp`（共 8 文件，其中 `*to_scene*` 过滤命中 20 用例全过）。
  结论：逐图种迁移 + 单测已由并行 Agent 实质完成，本回合只做**据实更正**（backlog 落后于真实代码，
  以代码为准）。`78_chart_scene` 的三条验证线（指令种类与分组 / render_scene 落格 / SVG 元素）仍可作
  后续抽查基线。
- [x] **已知遗留小项**（2026-10-28 收尾核实）：
  ① `data::rolling` O(N·window) → **已过时**：`rolling.cpp` 在 2026-10-22 已重写为 O(N)
  滑动窗口家族（SlidingSum / SlidingWelford / MonotonicQueue / OrderedMedian），Mean/Sum/Stddev/
  Min/Max/Median 全为 O(N) 或 O(N·logW)；本轮仅修正 `rolling.hpp` 头部过时注释（曾写"O(N·window)、
  刻意不展开"）。
  ② `math::interpolate` 的 monotone (PCHIP) 变体 → **本轮新增**：`interpolate.hpp` 加 `PchipSpline`
  （Fritsch–Carlson 单调三次 Hermite，保单调不过冲，接口对齐 `CubicSpline`），3 个回归测试
  （过点/线性精确、保单调不过冲、坏输入 fail-loud）全过。
  ③ `io/csv` 与 `dsl/csv_sniff` 合并 → **刻意不合并，记为结构性延迟**：`io`(L2) 数值快路径与
  `dsl`(L17) 方言分析+`document::TableModel`(L8) 组装是两份**有意为之**（L2 不能 include L8，
  层号硬墙）；两文件头注释已互相交叉说明。真要合并需先把 csv_sniff 的 TableModel 组装层拆到 L8+，
  属独立结构任务，未动。
  ④ `chart` 对 `data`/`io` 的消费接线 → **已落地**：`examples/76_csv_to_chart.cpp` 已是完整
  "CSV→fill_missing→EMA(5)→终端图"端到端示例（io/csv + data/fill_missing + data/rolling + math/stats
  + layout/raster/canvas/render/api 全链路），确定性可被 ctest 跑。

## 二、工程 / CI 设施（ROADMAP 阶段 0 未勾选项）

- [x] `.github/workflows/ci.yml`（2026-10-13 核实）：文件**确实已建**，含 guardrails / linux /
  windows 三作业；本轮补上 `gen_fixtures` / `gen_single_header` / `check_headers` 三条护栏，
  并新增 **tsan** 与 **coverage** 两个 ubuntu 作业（asan 作业此前已有）。
- [x] 代码风格 / 格式化配置（2026-10-13）：新增 `.clang-format` / `.clang-tidy` /
  `.cmake-format.yaml` / `.editorconfig` / `.gitattributes`；`.gitignore` 已存在（本轮补
  `site/`、补丁残留、编辑器本地覆盖三类）。
  **定位是"编辑器与增量改动用"，不进 CI 强制**——310 头全量格式化会产出上万行无关 diff。
- [x] `scripts/check_headers.sh` / `check_generated.sh` / `check_license.sh` 封装（2026-10-13）：
  三份已建并各自跑通（`check_license` 通过；`check_generated` 暴露出 `single_include` 与
  `结构.md` 两份产物过期，已重跑生成器修复）。
- [x] **sanitizer / coverage 确权**（2026-10-13）：结论分两半——
  * **本机（MinGW-w64 GCC 15.2）跑不了 ASan/UBSan/TSan**：该工具链**不带
    `libasan` / `libubsan` / `libtsan`**（链接期 `cannot find -lasan` 实证），这是平台限制
    不是代码问题；
  * **coverage 在本机可跑**：插桩构建 + `ctest` 全绿，但 gcov 运行时**写不进含非 ASCII
    字符的路径**，设 `GCOV_PREFIX` 指向 ASCII 目录后正常产出 382 份 `.gcda`；
  * 因此把 asan / tsan / coverage 三项都挂进 CI 的 **ubuntu** 作业（本次新增 tsan + coverage），
    让"全绿"这件事在 CI 上可验证，而不是只在本地口头声称。
- [x] **CI 三平台矩阵**（2026-10-21）：ci.yml 补 **macos (clang, macos-14)** 作业
  （`cmake --preset ninja` + 全量 ctest）。核实后确认风险两点均不存在：仓库无任何目标启用
  `-Werror`（`TERMCHART_WARNINGS_AS_ERRORS` 默认 OFF 且无调用方传 `WERROR`），clang 的
  `-Wconversion` 新告警不会炸构建；`TERMCHART_BUILD_PYTHON` 默认 OFF，CI 不依赖 pybind11。
  Linux 早在 2026-10-13 即在矩阵内，macOS 是此前的唯一缺口。
- [x] **WASM (Emscripten) 目标**（2026-10-28 落地脚手架）：此前"未开工"，现补齐最小可用骨架：
  * `src/wasm/termchart_wasm.cpp`：Emscripten 胶水，**只在 Emscripten 工具链下编译**
    （顶层 CMakeLists 用 `if(EMSCRIPTEN AND TERMCHART_BUILD_WASM)` 门控，桌面/CI 不碰）；
    出口 `termchart_blit_canvas(rgba,w,h,channels)`（EM_ASM 调 `putImageData`，零 embind）
    + `termchart_render_demo()`（渐变 + 正弦示例 Bitmap 推送）。
  * `examples/wasm/shell.html`：Emscripten `--shell-file`，含 `<canvas id="canvas">` 与
    "Render demo" 按钮（`Module.ccall('termchart_render_demo','void',[],[])`）。
  * `cmake/TermchartOptions.cmake`：`TERMCHART_BUILD_WASM` 开关（默认 OFF）。
  * `CMakePresets.json`：`wasm` 配置/构建预设（`-DCMAKE_TOOLCHAIN_FILE=$env{EMSDK}/...`、
    `TERMCHART_BUILD_WASM=ON`、关测试/示例/bench 以省 Emscripten 链接成本）。
  验证（2026-11-06 首次真编译，emsdk 6.0.13 / clang，Windows + Ninja）：`cmake --preset wasm`
  产出 `termchart_wasm.html/.js/.wasm`，348 个 TU 全过；在 node 里伪造 canvas 上下文实调
  `termchart_render_demo()`，拿到 480×240×4 = 460800 字节、左上 `[0,0,255]`、中心 `[127,255,127]`，
  与渐变公式一致。首编暴露并修掉三个**只有真编译才现形**的缺陷：
  ① `EM_ASM` 是普通可变参宏，预处理只按圆括号配对、不认花括号——块内裸逗号
     （`var w=$1, h=$2, c=$3;`）会切断宏参数，剩余部分被当 C++ 解析，已改为一行为一声明；
  ② `add_executable` 在 Emscripten 下默认后缀 `.js`，此时 `--shell-file` 被 em++ **静默忽略**
     （只给一条 `-Wunused-command-line-argument`），模板等于没接；已设 `SUFFIX ".html"`；
  ③ `"SHELL:-s EXPORTED_FUNCTIONS=['_a','_b']"` 在 Windows + Ninja 下外层单引号被剥掉，
     emcc 收到 `[_a,_b]` 解析失败后静默忽略，产物零自定义导出；已改紧连逗号写法
     `-sEXPORTED_FUNCTIONS=_a,_b`。另注意 `-O3` 下 binaryen 会压缩 wasm 导出名（`f`/`g`），
     js 侧靠 `Module["_termchart_blit_canvas"]=wasmExports["f"]` 映射表保证 `ccall` 可用。
  编译缓存：`wasm` 预设的 `environment` 里设 `EM_COMPILER_WRAPPER=ccache`（Emscripten 官方支持的
  ccache 包装方式，实测 100% cacheable），配合 ninja 满核并行。
  **CI 状态更正（2026-11-13 联网复核）**：WASM **已并入 CI** ——
  `.github/workflows/ci.yml` 有独立 `wasm (emscripten 真编译)` job（setup-emsdk +
  `cmake --preset wasm` + `test -s build/wasm/termchart_wasm.wasm`），2026-11-09 的
  ci run 中该 job 为 success。上面「刻意不并入 CI」是 2026-11-06 首编时的决定，
  已被后续改动覆盖；本行保留作历史痕迹。

## 三、质量与一致性（精简，逐条证据见 `quality_todo.md`）

- [x] **P1 API 一致性**（2026-10-13，**以兼容方式收敛，未做破坏性改名**）：
  * 数据动词：确立并文档化"`add_*` = 追加、`set_*` = 替换"，给三个只有 `add_*` 的图种
    （line / scatter / streamgraph）补上 `set_series()` + `series()`；
  * `api::Opts` → `ApiOptions`（全库一律 `XxxOptions`），`Opts` 保留为**兼容别名**；
  * `Renderer` 可移动（unique_ptr + 移动构造/赋值，见下条 P3）；
  * 默认配色收敛为 `chart::default_color(DefaultColor::*)`（**色号逐字不变**，
    只是从 93 处硬编码变成一个命名真源）；
  * `FunnelChart` 选项改为 `set_options()` / `options()`（与其他图种一致）；
  * `Graph::find_edge(from, to)` 补齐（此前只有节点能按 id 找回）。
  * **仍待拍板的破坏性部分**：`Graph::add_node` 返回引用 vs `add_edge` 返回指针的**返回类型
    统一**（7 处调用点需改）、`TERMCHART_API` 标记的全局统一。二者都会动到对外签名，
    需你先定方向再动手。
- [x] **P3 现代 C++ 毛刺**（2026-10-13 全部修掉）：`Renderer` 裸 pimpl → `std::unique_ptr<Impl>`
  + 移动语义；`CellBuffer::at()` 加 Debug 断言并新增 `try_at()`；`dsl` 8 处 `const_cast` 由
  新增的**非 const `find_node` / `find_participant` 重载**替掉（const_cast 只留在实现内 1 处）；
  `colormap::StopCache` 改为 **append-only 的 shared_ptr 缓存**（此前 `table[slot]=std::move(...)`
  会让已发布的 span 悬垂），并给 `sample()` 加枚举范围校验；
  `Canvas::put` / `CellBuffer::set_sides` 由"静默丢弃"改为**返回是否落格/写入**。
- [x] **P4 obstacle 横挡绕障**（2026-10-13 已修，回归测试锁定；见第一节）。
- [x] **P4 metro 直线兜底不穿框**（2026-10-13）：路由新增第 3.5 步 **Z 形绕行**
  （轴对齐三段，候选行/列为两端点与中点 ±1..max_detour 的确定性枚举），把跨链边从
  "直线兜底（可能穿框）"里救回来；新增 `graph/route_metro: 跨链边靠 Z 形绕行避开节点框`
  回归测试，对**跨链边**也断言了"中间格不落在任何节点框内"。
- [x] **P4 dsl ensure() 超限后仍造节点**（2026-10-13）：`d2` 与 `mermaid_sequence` 超限报错后
  不再调用 `ensure_*`，改为返回不进图的"黑洞"对象——此前 `nodes.size()` 会越过上限，
  边界断言形同虚设。
- [x] **P4 清理临时调试文件**：`tests/unit/graph/_dbg_metro.cpp` 已不在工作区（此前已删）。

## 四、文档 / 口径同步（P0，零风险）

> phase7 落地 `dsl`/`widget`/`plugin`/`app`/`diagrams`/`single_include` + 大量 examples 后，下列文档数字曾失真并已在当时刷新（`capability_matrix` 自身要求"改动后重跑命令再更新"）。
> **2026-11-16 按当前源树重数**：`include/termchart/**/*.hpp` **416 头**、`src/**/*.cpp` **369**、
> `examples/*.cpp` **87**、`tests/**/*.cpp` **204**、golden 14 份；`ctest` **95/95 全通过**、
> doctest **1494 用例 / 267222 断言全通过**、`check_all` **453 头独立编译**全绿、
> shared 构建链接 + 同一套用例通过、`v1.0.0` tag 在。
> 下列 `[x]` 条目中的 333 头 / 282 实现 / examples 57 等是 2026-10-13 的历史值，不作现状引用。

- [x] `capability_matrix.md`：第五节"规格差距"按实测刷新（333 头 / 282 实现、缺口 ~26–28、
  examples 57、single_include 已建立、bench 2→3、fuzz 8、fixtures 7/7）；
  第一节 `widget`/`plugin`/`app` 三行由"空"改为真实内容；开篇数字口径、示例索引标题同步。
- [x] `ROADMAP.md` 现状节"未落地"清单：`dsl`/`widget`/`plugin`/`app` 改为"已落地"。
- [x] 阶段 0 任务清单与出口条件：ci.yml / 格式化配置 / shell 封装三项勾选状态已与真实情况对齐。
- [x] **`capability_matrix.md` / `ROADMAP.md` 登记 `chart/geo`（2026-10-28 收尾）**：`capability_matrix`
  第五节模块表加 `chart/geo` 行（登记 GeoMap 4 投影 / 3 字符出口 + 位图 / 图例色标 / `hit_test` 命中、
  `assets/maps` 离线数据随安装包分发），并把 `chart` 行原有 "geo 4/4" 字符图种提及交叉引用到该行；
  示例索引补 `67_world_map` / `68_cn_province` / `69_city_plot`；`ROADMAP` 修订记录加 2026-10-28 一行。
  `docs/charts/geo.md` 此前已同步更新（口径从"库不做投影/不带数据"改为现状）。

## 五、GUI 平行项目（已完成，2026-10-28）

- [x] `termchart_gui`：平行栈已落地（详见 `capability_matrix.md` 第六节与 `ROADMAP` 修订记录）。
  渲染核心 `include/termchart/gui/`（`font` / `scene_renderer` / `hover_chart`）+ 并列顶层 header-only
  库 `include/termchart_gui/`（layout / widget_tree / native_loop / backend_tk / render，20+ 头）。
  其中 **`backend_tk` 提供 `Backend` 抽象 + `HeadlessBackend`（`BitmapCanvas` + 内部事件队列）**，使整条栈
  能在无窗口系统下编译、测试——`tests/unit/gui/test_gui_stack.cpp` 共 40 用例 / 181 断言（layout 重排、
  widget 命中/回调、ChartWidget 悬停/缩放/淡入、Combo/Gauge/Polar 渲染均落真彩像素断言）**2026-10-28 复核全过**。
  **真实落地边界（据实更正）**：原生窗口后端 `Win32Backend` / `QtBackend` / `TkBackend` / `X11Backend` /
  `CocoaBackend` / `JniBackend` 的完整实现位于 **`plugins/gui_win32` / `gui_qt` / `gui_tk` / `gui_x11` /
  `gui_cocoa` / `gui_jni`**，**不在** `include/termchart_gui/` 内；`termchart_gui::Backend` 接口默认只有无头
  实现，真实事件循环需宿主启用对应插件（桥接需 Tk/Qt/Win32/Xlib/AppKit/JRE 运行时）。
  **2026-11-11 新增三套 Take 2「句柄注入」后端**（窗口归宿主，后端只贴 `BitmapCanvas` + 收剥壳事件）：
  `gui_x11`（`XPutImage` 上屏，按 15/16/24/32 位深打包，`x11_smoke` 在 Xvfb 下通过）、
  `gui_cocoa`（宿主 `drawRect:` 里画 `CGImage`，按 `isFlipped` 决定是否翻 Y）、
  `gui_jni`（`jintArray` ARGB 回写，真 JVM 下 `jni_smoke` 通过）。三者由顶层按平台自动纳入
  （Unix→X11、Apple→Cocoa、有 JDK→JNI），缺依赖时插件自行 `STATUS` 跳过，不再是开关 + fail-loud。
  **Cocoa 真编译状态（2026-11-13 更正）**：顶层 `CMakeLists.txt` 在 `APPLE` 下自动
  `add_subdirectory(plugins/gui_cocoa)`，插件 CMake `enable_language(OBJCXX)` 编译
  `cocoa_backend.mm` 并注册 `cocoa_offscreen` 无窗口冒烟；CI `macos` job 已加显式断言
  （静态库产物存在 + 用例已注册 + 冒烟通过），不再是「只做过静态审查」。
  本机无 macOS，且最新 CI run 早于 11-11 的 Take 2 改动，故仍待**下一次推送触发的
  macOS CI 实际跑过**才算闭环。
  **Qt 侧 2026-11-07 已真编译 + 冒烟验证**：Qt 6.8.3（`mingw_64` 动态构建）、GCC 15.2，
  `-DTERMCHART_BUILD_GUI_QT=ON` 下 355 个 TU 全过，产出 `examples/qt_demo.exe`，`--smoke` 输出
  `frame rendered, window closed cleanly`；`objdump -p` 依赖为 `Qt6Core/Gui/Widgets.dll`（**动态链接**）。
  首编暴露并修掉三个「从未编译过」才现形的缺陷：① `QWheelEvent` **从来就没有** `localPos()`
  （Qt5 是 `posF()`、Qt6 是 `position()`），原实现照抄鼠标事件的写法，Qt5/Qt6 都编不过；
  ② `CanvasWidget` 无 `Q_OBJECT` 却 `#include "qt_backend.moc"`（AUTOMOC 不会为无 `Q_OBJECT` 的 TU 生成 .moc）；
  ③ 事件回调对**任何**事件都调 `QApplication::quit()`，`run_loop` 模式下首次鼠标移动/尺寸变化即整个进程退出。
  **许可边界（重要）**：Qt 是 **LGPLv3 / GPLv3 / 商业** 三重授权，而本项目本体是宽松许可。只允许**动态链接**——
  `plugins/gui_qt/CMakeLists.txt` 在配置期对静态 Qt `fail-loud`（静态链接会触发 LGPLv3 §4(d) 的可重链接义务，
  转嫁给所有下游分发者）；**不 vendoring** 任何 Qt 二进制，一律 `find_package` 由使用者自备；分发含该后端的
  可执行文件时须附 LGPLv3 全文与 Qt 源码获取途径，并保证终端用户能替换 Qt 库本身。**不要**为省事把本项目改 GPLv3。
  GUI 复用边界是 `scene`(L4) 及以下，不碰终端呈现（`cell`/`SGR`/`CellBuffer`）。
- [x] **Python 桥强化 + Qt Take 2（2026-11-13）**：
  `bindings/python/termchart_pybind.cpp` 重写输入层——`x/y/values` 统一接受
  list / tuple / **numpy.ndarray（零拷贝读缓冲，多 dtype：f32/f64/int/uint）** / **pandas
  Series·DataFrame 列（`.to_numpy()`，运行时 duck-typed、零编译期依赖）**；新增
  `plot_df(df,x,y)`、`plot_series(s)`；新增像素输出 `to_rgba/to_rgb`（numpy `(H,W,3/4)` 数组）
  与 `to_rgba_bytes/to_rgb_bytes`（bytes，无需 numpy）、`to_png_bytes`、`save_png_pixels`
  （bytes/ndarray → PNG，给 Tkinter/PIL/Qt 再合成用），并对光栅类函数加
  `py::call_guard<py::gil_scoped_release>()` 释放 GIL；配套 `examples/79_tkinter_demo.py`
  （PIL → Tk PhotoImage 嵌进 Tk 窗口）、`examples/80_qt_demo.py`、`bindings/python/termchart.pyi`
  已同步。**`gui_qt` 接 Take 2 句柄注入**：`QtBackend(const NativeSurface&)` + `blit(QPainter&)` /
  `frame()` / `resize()`（宿主 QWidget 在自己 `paintEvent` 里 `blit`，与 Win32/Cocoa/X11/JNI 同构），
  另加 `static show_bitmap()` 一键开 Qt 看图窗口；对应 `example/embed_demo_qt.cpp`；Python 侧
  `show_qt` 在 `TERMCHART_BUILD_GUI_QT=ON` 时由 `bindings/python/CMakeLists.txt` 自动链入并暴露。
  **未编译验证**：本机无 Qt，Qt 后端 Take 2 / `embed_demo_qt` / Python `show_qt` 仅通过代码审查，
  与 Cocoa 同档（待带 Qt 的构建/CI 闭环）。其余 Python 桥改动待装 numpy/pandas 后本地编译 + 跑示例闭环。
- [x] **Rust 桥 / Java 桥（仅规划，未实现）**：现状与规划归属已统一收拢到**第七节**（当前真缺口清单），
  此处不再单列未勾项以免口径分叉。方案见 `docs/design/ffi_plan.md`。
- [x] **`single_include/termchart.hpp` 必须与公开头改动成对提交（工程护栏，长期纪律）**：改公开头（如
  `include/termchart/chart/series.hpp` 的 `set_points` 移动语义）后必须重跑 `tools/gen_single_header.py`
  并重生成 `single_include/termchart.hpp`，二者**一并暂存提交**；若只提交公开头、漏掉重生成的单头，
  CI `check_generated`（guardrails 套件）会红。2026-11-13 并行 Agent 改动中 `series.hpp` 已改、单头已本地
  重生成但未替其提交，提交时务必捆绑。

## 六、地理图（`chart/geo`）与地图数据链（2026-10-18 登记）

> 背景：`examples/67_world_map` / `68_cn_province` / `69_city_plot` 原先是"96×28 文本格 + 非 ASCII 全变 `?` + 丢 Style"，
> 本轮改为真 GeoJSON 栅格化（盲文 2×4 / 象限 / 半块三条出口 + 分级着色 + 位图/PNG）。过程中修掉
> `tools/topojson_to_geojson.py::stitch_arc` 的两处解码错误（负索引必须取 `~idx`；必须**先**差分解码**再**反序），
> 世界地图从"乱色块"恢复为可辨认形态。下列是这次改造暴露、但本轮**没做**的项。

- [x] **TopoJSON 缝合离线回归（2026-10-27 补）**：`stitch_arc` 的两处解码错误（负索引必须取
  `~idx`；反序必须在 delta 解码之后）此前**不报错、只产出合法 JSON**。现已加最小夹具
  `tools/tests/topojson_stitch_sample.json`（1 条负索引反向弧 + 1 条正向弧、2 个 feature）+
  独立可运行回归测试 `tools/tests/test_topojson_stitch.py`（4 个用例：直接 stitch_arc 正反路径、
  负索引解析到正确弧、端到端 convert 几何）。并接入 CI `guardrails` 作业，不再只靠联网自检。
  **验收已做突变验证**：把 `~idx` 改回 `idx` 后测试确实变红，红线生效。
- [x] **`GeoMapOptions` 三个死字段（2026-10-28 收尾）**：`show_graticule` 全仓零消费者；`width` / `height`
  在 `geo_map.cpp` 里没有任何 `options_.width` 读取（实际尺寸只认 `DrawContext` / `rasterize` 入参）。
  已**删字段**（含 `single_include/termchart.hpp` 镜像），并在结构体注释说明画布尺寸由出口参数决定。
  验收：`grep -r show_graticule src/ include/ single_include/` 已无任何命中（提交 `488ae95`）。
- [x] **分级 `bins` 统一到三条出口（2026-10-27 收尾）**：抽出 `geo_bin_of` / `geo_bin_center` 单一分级函数，
  `value_color` 接收 `bins` 参数；`draw()` / `to_scene()` 内联分级改为调用 `geo_bin_center`，
  `rasterize()` / `render_bitmap()` 的 `value_color` 调用接入 `options_.bins`。同值在三出口取同一级色。
  新增回归用例（bins=2 同 bin 同色、bins=1 塌缩同色）。提交 `10d3c34`。
- [x] **图例 / 色标（2026-10-28 收尾）**：新增 `legend_bins(kind)`（按 `options_.bins` + 值域生成分级图例，
  项名形如 `lo–hi`）+ `render_legend(...)`（复用 `layout::draw_legend` 产字符格）；`render_bitmap` 新增
  `GeoBitmapOptions::draw_legend`，开启时右侧留白带画纵向色标（高值在上），与 `legend_bins` 共用 bins/色带。
  4 个回归测试。提交 `58b8a33`。
- [x] **`HitRegion` 出口（2026-10-28 收尾）**：新增 `hit_test(lon, lat)`，even-odd 射线法在原始经纬度环上做
  点在多边形内判定，返回最上层 feature 名（不在任何要素内返回 `""`）。这是命中交互的基础——GUI 把屏幕格
  反投影到经纬度后调用即可取回要素名/数值，与投影/渲染方式无关。回归测试覆盖 A 上/B 下方块命中。提交 `488ae95`。
  （注：未直接产出 `chart/hit_region.hpp` 的 `HitRegion` 结构体；`hit_test` 是更底层、投影无关的坐标命中原语，
  上层交互按需包成 HitRegion。）
- [x] **安装包分发地图数据（2026-10-28 收尾）**：`assets/maps/{world,cn,cities}.json` 已进 git，现新增
  `install(DIRECTORY assets/maps DESTINATION ${CMAKE_INSTALL_DATADIR}/termchart)`，随库安装分发，GeoMap
  可离线加载。提交 `6081fce`。
- [x] **`feature_names()` 有损去重（2026-10-28 收尾）**：已在 `feature_names()` 实现处补注释，明确"同名不同
  feature 会被合并、不可当唯一键；需逐 entry 请用 `polygon_count()`"。当前三份数据无同名冲突，够用。

---

## 七、2026-11-16 复核后仍在的真缺口（唯一未做清单）

> 除以下 1 条外，ROADMAP 历史 29 条 `[ ]` 与本节原 6 条缺口均已按代码实证核销/改勾（详见 ROADMAP 2026-11-16 两条修订记录）。
> 本节是「当前真缺口」的唯一权威小节；前述各节 `[x]` 条目中的历史数字不作现状引用。
> 实测数字（2026-11-16 三轮收口后）：416 头 / 369 cpp / 87 示例 / 204 测试源 / ctest 95/95 / doctest 1494 例全过。

- [x] **HEVC 多参考帧**（2026-11-16）：`hevc_multi_ref` 默认 false，L0=[POC-1, POC-2]，RPS 两负参考 + `ref_idx_l0` + DPB 两帧滚动 + merge/AMVP 带 ref_idx；`verify_hevc_multiref.py` 12/12 `neq=0`、ref=1 选中率 85.7%、cycle2 码率 −71.4%，接 CI。
- [x] **HEVC 双向预测（B 片）**（2026-11-17）：GOP=3 重排（I0 P2 B1 P4 B3…），L0/L1 各 1 帧，SPS/VPS `max_num_reorder_pics=1` 保证解码端按 POC 输出；slice header 的 l1 计数 / `mvd_l1_zero_flag`、B CU 的 `inter_pred_idc` 与双向 bin 序列落地；`tools/verify_hevc_bslice.py` 7 形态 × QP10/26 全绿。
- [ ] **WONTFIX / Backlog：HEVC 混合 B AMVP**（非 merge 路径）：同一 B 片内混用 merge 与 L0/L1/BI AMVP 时，跨 POC 邻居候选推导精度未与 FFmpeg 完全对齐，部分运动形态下 bin 分叉。决策：**不继续收敛**，默认 `hevc_b_slice` 走 merge-only（仍含 combined bi-pred 双向候选，像素级 100% 对拍通过）；`EncodeOptions::hevc_b_amvp` 保留为实验开关。Why：解除 AV1 开工前的红灯锁定；How to apply：不要再为混合 AMVP 开新战线，除非用户明确重启。
- [x] **`bench_raster` 性能基线**（2026-11-16）：`bench/bench_raster.cpp`（五光栅器 × 三尺寸）+ `bench/baseline.json` 实测数据，`--quick` 进 ctest。
- [x] **`test_roundtrip_scene` / `test_headless_pipeline` / `bench_diff`**（2026-11-16）：三个集成测试/基准落地，Scene 往返 + headless 三段 + diff 三档。
- [x] **`interaction > graph` 端到端验证**（2026-11-16）：`test_interaction_graph.cpp` 建图→布局→`HitRegion`→`topmost_hit` 回指正确节点。
- [x] **app 循环三测**（2026-11-16）：`test_app_loop` / `test_thread_post` / `test_resize` 三文件落地。
- [x] **Rust / Java 桥**（2026-11-16 落地）：前置 C ABI `termchart_capi` 早已在 v1.1 实现。
  - **Java**：`bindings/java`（`Termchart` + `TermchartException` 高层出图类 + JNI glue），自包含 `termchart_jni.dll`（编入 capi.cpp + 静态 core/运行时），`java_smoke` 真 JVM 冒烟通过；`TERMCHART_BUILD_JAVA=ON`。
  - **Rust**：`bindings/rust`（`termchart-sys` 手写 FFI 免 bindgen + `termchart` 安全封装，一行出图/Chart RAII/SVG·PNG 两段式导出）。本机无 cargo，源码已落地，**待 CI/Rust 工具链 `cargo build`+`cargo test` 闭环**；实现取舍见 `bindings/rust/README.md`。
  - 方案记录见 `docs/design/ffi_plan.md`（已降级为设计记录）。

---

### 说明
- 质量专项（P1/P3/P4 的证据与逐条改法）在 `docs/reference/quality_todo.md`，本文件只做总览。
- 本清单与 `quality_todo.md` 并列，两者索引都归 `ROADMAP.md`（唯一进度口径）。

---

## 八、AV1 全量规划：第一阶段范围与暂缓清单（2026-11-17 拍板）

**拍板口径**：先 8-bit 4:2:0；运行期零依赖，dav1d / libaom 只在测试期作为参考解码器引入；第一阶段目标是最小纵切「8×8 DC 帧内 + DCT8 + 最小 OBU/TileGroup，经 dav1d 真解出且 recon==dec」，而不是横铺空模块。规范真值 = AV1 规范文本 + 参考解码器 + 官方 conformance vectors，三者冲突时以可解码性与向量为准。

**第一阶段（在办）**：
- [ ] OBU 头 + TEMPORAL_DELIMITER / SEQUENCE_HEADER / FRAME_HEADER / TILE_GROUP + IVF 封装
- [ ] Daala 算术编解码 + 静态 CDF（不更新）自闭环回环
- [ ] 8×8 PARTITION_NONE + DC_PRED + DCT8 + 量化/反量化/反变换 + 最小 TileGroup
- [ ] `dav1d` 解码对拍门禁：recon==dec，逐像素

**暂缓（按约束分层，全量规划不丢项）**：
- [ ] 熵/CDF：CDF 自适应更新、多符号 CDF 树全量语法上下文（划分/模式/MV/残差全体）
- [ ] 划分：128×128 超级块、其余 9 种 partition（HORZ/VERT/SPLIT/HORZ_A/B/VERT_A/B/HORZ_4/VERT_4）与跨 SB 邻居推导
- [ ] 帧内：细角度子集 13..68（69 种模式中的 56 种 finer angles）、Smooth/Paeth 全集、Filter Intra、CFL、帧内边界滤波
- [ ] 帧间：7 参考槽、8 插值滤波器、全像素/亚像素 ME、Warped Motion（平移/旋转缩放/仿射）、OBMC、复合预测（Average/Distance/Wedge 16 掩膜/Difference）、Masked Compound
- [ ] 变换：ADST 4..16、Identity 4..64、2..64 各尺寸组合、量化矩阵 15 预设、复杂扫描序
- [ ] 环路滤波：Deblocking 精确强度、CDEF（8 方向搜索 + 查表）、Loop Restoration（Wiener / Sgrproj 定点化）
- [ ] 系统层：tile groups 多 tile、帧率/OrderHint/呈现时间、HDR/10-bit/12-bit、4:4:4、film grain、SVC/可伸缩、MP4 `av01`/`av1C` 封装
- [ ] RDO：划分/模式/参考/复合模式的全量 RD 决策与提前终止（仅画质工具，合规链打通后再做）
