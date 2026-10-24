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
- [ ] **SymLog（对称对数）轴**（2026-10-24 登记，长期，**明确不在近期做**）：`scale` 只有
  线性与对数（对数轴拒非正值），没有 `symlog`（近零点线性、远处对数）。因此"0.001~1e21 跨十几个
  数量级且含负值"这类列，目前靠 `autoviz` 的 Tukey 篱裁剪把极值钳到轴边缘处理（见
  `docs/guide/autoviz.md` 的「极端离群值」）。真要做需三件套：`SymLogScale`（`linthresh` /
  `linscale`）+ 跨零点的刻度生成（主要风险点：tick 序列与网格对齐）+ `layout` 的零线/负区约定。
  **决策依据**：裁剪已覆盖真实脏数据场景（量级是"脏"而非"信号"），为单一图种动 `scale` 与 22 层
  架构的收益不划算；触发条件是出现"量级本身就是业务信号"的真实需求。
- [ ] **已知遗留小项**（2026-10-21 登记）：① `data::rolling` 的纯 SMA 仍 O(N·window)，
  滑动求和 O(N) 化属局部优化（EMA 已 O(N)）；② `math::interpolate` 的 monotone (PCHIP)
  变体未做（自然样条在噪声数据上轻微过冲）；③ `io/csv` 与 `dsl/csv_sniff` 两份解析器
  的合并需先把 csv_sniff 的 document::TableModel 组装层拆到 L8+（层号硬墙），记后续；
  ④ `chart`(L12) 对 `data`(L4)/`io`(L2) 的消费尚未接线（"CSV→均线→出图"端到端示例）。

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
- [ ] **WASM (Emscripten) 目标**（2026-10-21 登记，未开工）：`HeadlessBackend` 产出的
  `image::Bitmap` 加 Emscripten 胶水（`encode_png` → `<canvas>` drawImage / WebGL 纹理）
  即可刷网页，渲染核心（scene/gui/SceneRenderer，L4 及以下）无需改动；主要成本在
  构建侧（`-s USE_PTHREADS`、文件系统虚拟化、`term/` 平台代码在无 tty 环境的退化已验证），
  以及一个 `<canvas>` 示例与 `emcmake` preset。刻意不并入 CI（runner 生态差异大），
  留作发布渠道工作。

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
- [ ] **`capability_matrix.md` / `ROADMAP.md` 未登记 `chart/geo`**（2026-10-18）：两处全文无 `GeoMap`，
  而实际已有 `GeoMap`（4 投影 / 3 条高清出口 / 位图）+ `examples/67–69` + `assets/maps/`。
  需补：`capability_matrix` 第五节模块表加 `chart/geo` 行（5 个图种）、示例索引加 67–69；
  `ROADMAP` 修订记录加一行。`docs/charts/geo.md` 已在本轮同步更新（口径从"库不做投影/不带数据"改为现状）。

## 五、GUI 平行项目（不阻塞库本体）

- [ ] `termchart_gui`（`结构.md` 标为平行项目）：`include/termchart_gui/` + `src_gui/` 共 25 文件未开工（GUI 复用 `scene` 及以下 L0–L4，`widget`/`app`/`plugin` 在 GUI 下重写）。详见 `capability_matrix.md` 第六节。**进展（2026-10-14）**：渲染核心以 `include/termchart/gui/`（层 22）先期落地——`font` + `scene_renderer`（Scene→Bitmap，3× 超采样 AA）+ `hover_chart`（悬停出数据，`examples/58`）+ `examples/55`；widget_tree/native_loop/layout/backend_tk 的骨架已在平行栈 `include/termchart_gui/` 落地（真实 tk 后端未接）。

## 六、地理图（`chart/geo`）与地图数据链（2026-10-18 登记）

> 背景：`examples/67_world_map` / `68_cn_province` / `69_city_plot` 原先是"96×28 文本格 + 非 ASCII 全变 `?` + 丢 Style"，
> 本轮改为真 GeoJSON 栅格化（盲文 2×4 / 象限 / 半块三条出口 + 分级着色 + 位图/PNG）。过程中修掉
> `tools/topojson_to_geojson.py::stitch_arc` 的两处解码错误（负索引必须取 `~idx`；必须**先**差分解码**再**反序），
> 世界地图从"乱色块"恢复为可辨认形态。下列是这次改造暴露、但本轮**没做**的项。

- [ ] **TopoJSON 缝合没有离线回归（优先）**：`stitch_arc` 的两个错误都**不报错、只产出合法 JSON**，
  肉眼只能看出"地图不像地球"。目前唯一防线是 `fetch_map_data.py` 抓完立刻自检（要联网、只覆盖当下那份数据）。
  应加一份最小夹具（1 条负索引反向弧 + 1 条正向弧、2 个 feature）与用例；验收＝把 `~idx` 改回 `idx` 后测试必须变红。
- [ ] **`GeoMapOptions` 三个死字段**：`show_graticule` 全仓零消费者；`width` / `height` 在 `geo_map.cpp` 里
  没有任何 `options_.width` 读取（实际尺寸只认 `DrawContext` / `rasterize` 入参）。要么接上（画经纬网、
  作为 `draw()` 覆盖值），要么删字段并写进 CHANGELOG。验收＝`grep -r show_graticule src/ examples/` 有真实用途，否则字段消失。
- [ ] **分级 `bins` 只在高清路径之外生效**：`draw()` 按 `options_.bins` 量化，`rasterize()` /
  `render_bitmap()` 是连续映射（全文只有 `draw()` 里那两处 `bins`）。高清是主打出口，应把"值 → 级 → 色"
  抽成一个函数三处共用，并补"同一份值在两种出口取到同一级色"的用例。
- [ ] **没有图例 / 色标**：`bins` 分级没有 legend，看不出颜色含义。`layout/legend` 已有能力可直接复用：
  字符格出口追加几行"级 → 颜色 + 区间"，位图出口在 `--png` 右侧留白。
- [ ] **没有 `HitRegion` 出口**：GeoMap 不产出 `chart/hit_region.hpp` 的命中区域，`interaction` 的 tooltip /
  `legend_toggle` 接不上（69 的城市气泡目前无法悬停出数值）。
- [ ] **安装包不带地图数据**：`assets/maps/{world,cn,cities}.json` 已进 git，但 `CMakeLists.txt` 只有
  `install(DIRECTORY include/ ...)`，没有 `share/termchart/maps`。要么补安装规则 + `maps_dir()` 查询，
  要么在文档里明确"数据不入安装包，取样请跑 `tools/fetch_map_data.py`"——二选一，不能两头空。
- [ ] **`feature_names()` 去重是有损的**：同名不同 feature（多份图层叠加）会合并成一个名字，`set_value`
  无法分别赋值。当前三份数据无同名冲突，够用；但要在头注释写明限制，避免以后有人拿它当"唯一键"。

---

### 说明
- 质量专项（P1/P3/P4 的证据与逐条改法）在 `docs/reference/quality_todo.md`，本文件只做总览。
- 本清单与 `quality_todo.md` 并列，两者索引都归 `ROADMAP.md`（唯一进度口径）。
