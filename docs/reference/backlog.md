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
  验证：本机无 emsdk，**未实际编译 WASM 目标**（仅确认普通 `cmake --preset ninja` 重配无错、
  新 src/wasm 不被普通构建误编）；需在装有 emsdk 的环境跑 `cmake --preset wasm && cmake --build --preset wasm`
  产出 `termchart_wasm.html` 后在浏览器打开。仍**刻意不并入 CI**（runner 生态差异大）。

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

> phase7 落地 `dsl`/`widget`/`plugin`/`app`/`diagrams`/`single_include` + 大量 examples 后，下列文档数字已失真，需重数刷新（`capability_matrix` 自身要求"改动后重跑命令再更新"）：

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
  **真实落地边界（据实更正）**：原生窗口后端 `Win32Backend` / `QtBackend` / `TkBackend` 的完整实现位于
  **`plugins/gui_win32` / `plugins/gui_qt` / `plugins/gui_tk`**（默认 OFF，仅因依赖外部运行时），**不在**
  `include/termchart_gui/` 内；`termchart_gui::Backend` 接口目前只有无头实现，尚未桥接到某个真实窗口事件循环
  （桥接需 Tk/Qt/Win32 运行时，无法在本机 headless / CI 验证，记为后续）。GUI 复用边界是 `scene`(L4) 及以下，
  不碰终端呈现（`cell`/`SGR`/`CellBuffer`）。

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

### 说明
- 质量专项（P1/P3/P4 的证据与逐条改法）在 `docs/reference/quality_todo.md`，本文件只做总览。
- 本清单与 `quality_todo.md` 并列，两者索引都归 `ROADMAP.md`（唯一进度口径）。
