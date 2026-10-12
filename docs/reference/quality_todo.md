# 质量收口清单（ROADMAP 子文档）

> 本文件是 `ROADMAP.md` 的「质量收口」子文档，索引见 ROADMAP 顶部 CHANGELOG（2026-10-13 一行）。
> 基于 2026-10-12 质量体检（架构 9/10、实现 9/10、API 美感 8/10、文档 8/10、测试 8/10、开发者体验 6/10）。
> 范围：抽样体检（审了 39 个公共头 + 25 个实现文件 + 对照 `capability_matrix.md`/`ROADMAP.md`/`CODEBUDDY.md`），非逐文件签字。
> 勾选规则同 `ROADMAP.md`：代码落地 + 测试通过 + 相关校验脚本通过，才打 `[x]`。

---

## P0 — 文档与真值同步（零代码风险，先做）

文档数字已严重脱节，等于"质量达标"的自我声明失真。文件自身要求"改动后重跑命令再更新"，但没执行。

- [ ] **刷新 `capability_matrix.md` 测试数字**：现状写的 `ctest 31/31 / doctest 497 用例 / 189357 断言` 已陈旧，实际 `termchart_tests` 已 **757 用例**且 graph 新模块还在涨；golden/fuzz/头数同步刷新。重跑 `ctest --preset ninja`、`check_all` 后回填（见该文件头部命令块）。
- [ ] **`ROADMAP.md`「一、项目现状」同步**：graph 现部分可用（metro/cluster/bundling/obstacle/dag/dependency 已落地），dsl 14/14 文件齐备，chart 68 文件齐备，interaction 可用——不再是"L17 dsl / L18 widget / L19 plugin / L20 app 零代码空洞"的旧口径（仅 widget/plugin/app/GUI 仍空）。
- [ ] **`capability_matrix.md` 模块表**：`graph` 行状态从"部分"细化，标出已落地子目录与剩余缺口（kamada_kawai、sugiyama 分层收尾等）。

---

## P1 — API 一致性（破坏性，需先拍板方向）

影响所有调用方与示例，改名前先确认方向。

- [ ] **统一 chart 数据灌入动词**：当前发散为 `add` / `add_series` / `set_values` / `set_data` / `set_stages` / `add_bubble_series`（证据：`chart/dataset.hpp:31`、`line_chart.hpp:20`、`bar_chart.hpp:21`、`scatter_chart.hpp:22`、`hist.hpp:23`、`funnel.hpp:28`、`boxplot.hpp:35`）。建议收敛为 `add_series` / `set_data` 两件套，或提供共享 builder。
- [ ] **顶层配置类 `Opts` → `*Options` 对齐**：`api/opts.hpp:19` 叫 `Opts`，而全库其他都是 `BrushOptions`/`CrosshairOptions`/`MermaidOptions`/`FunnelOptions`/`BoxPlotChart::Options`——最表层反而不统一。建议改名 `ApiOptions` 或 `QuickOptions`。
- [ ] **`Renderer` 不可移动**：`render/renderer.hpp:26-27` 删了拷贝却没声明移动，配合 `:46-48` 的裸 `Impl* impl_` → 按值返回/入容器都别扭。补 `Renderer(Renderer&&) noexcept` 与移动赋值（建议配合 P3 的 `unique_ptr` 改造一起做）。
- [ ] **公共 chart 入口 `TERMCHART_API` 标记不一致**：兄弟 chart 类型有的标 `TERMCHART_API` 有的没标（`bar_chart.hpp:21` `set_values` 等）。对全部公共 chart 入口统一加导出标记。
- [ ] **chart 默认配色统一**：同类默认色魔数各唱各调（`chart/series.hpp:15` `ansi16(7)` vs `bar_chart.hpp:22`/`hist.hpp:24` `ansi16(2)` vs `boxplot.hpp:30` `ansi16(4)` vs `funnel.hpp:19` `none()`）。抽统一 `kDefaultSeriesColor` 或默认走 `palette::series_color()`。
- [ ] **`FunnelOptions` 封装破裂**：`chart/flow/funnel.hpp:32` 直接暴露公有成员 `FunnelOptions options;`，兄弟类 `BoxPlotChart` 用 `set_options(Options)`（`boxplot.hpp:34`）。改为 `set_options`/`options()` 统一封装范式。
- [ ] **`Graph` 增操作返回类型不一致**：`graph/graph.hpp:26` `add_node` 返回 `Node&`、`:35` `add_edge` 返回 `Edge*`（可空）。建议统一返回 `NodeId`/`EdgeId` + `find()`，解耦生命周期风险。
- [ ] **`parse_mermaid` 出参风格**：`dsl/mermaid.hpp:50-52` 既返回 `Result<AstGraph>` 又带 `ParseDiagnostic* out_diagnostic` 出参，纯度不统一。建议把诊断并入错误变体。
- [ ] **`Opts` 访问器返回类型不对称**：`api/opts.hpp:24` `width()` 返回 `std::optional<uint16_t>`，`:35` `unicode()` 返回裸 `bool`。统一策略（要么都 optional 表示"未设置"，要么都给默认值裸类型）。

---

## P2 — 开发者体验（最大短板，最好修）

- [ ] **补"一行出图"入口**：文本侧有 `api::hello/print_line`（`api/quick.hpp:28-36`），但画一张 chart 仍需 `Canvas+Dataset+Chart+DrawContext+Renderer` 约 10+ 行。补 `tc::api::draw_chart(Chart&, …) -> Result<std::string>` 之类单调用，让新手 5 行出图。
- [ ] **写 Quick Start 文档**：24 个示例分散，缺"给我 5 行画折线"的零门槛路径（可放 `docs/guide/` 或 README 首屏）。

---

## P3 — 现代 C++ 人体工学毛刺（非缺陷，加分项）

- [ ] **`render/renderer.cpp:157-159`**：手写成对 `new Impl` + 裸 `impl_` 指针，误加拷贝会 double-free。改 `std::unique_ptr<Impl>` 或内联 `Impl`，析构 `= default`（与 P1 `Renderer` 移动改造合并）。
- [ ] **`core/buffer.cpp:24-30`**：`at()` 为 `noexcept` 且无边界检查，越界直接 UB。加 debug 断言或提供 `try_at()`。
- [ ] **`dsl/*` 多处 `const_cast<AstNode*>(find_node(...))`**：`d2.cpp:310` / `graphviz_dot.cpp:435` / `mermaid_flow.cpp:323` 去 const 再改。给 `find_node` 补非 const 重载。
- [ ] **`color/colormap.cpp:62-84,139`**：`StopCache` 返回指向 `table[slot]` 的 `span`，单线程安全但并发场景会悬垂；`kFits[static_cast<int>(kind)]` 无范围校验。缓存 key 纳入 `count` 或值返回；枚举下标前加范围检查。
- [ ] **`canvas/path.cpp:249` / `core/buffer.cpp:59`**：`put`/`set_sides` 越界静默丢弃/no-op，调用方无法分辨成功/丢弃。让它们返回 `bool` 或加 `assert`。
- [ ] **`graph/layout/metro.cpp:170-174`**：`expand_path` 中 `steps`(double) 经 `static_cast<int>` 循环，当前坐标受 `kMaxGridCoord` 约束故恰好不溢出，但无显式断言。改 `std::uint32_t` 或加 `assert(steps <= INT_MAX)`。

---

## P4 — 算法局限与测试契约

- [ ] **`obstacle` 横挡绕障真缺陷（非 best-effort）**：单折点 Z 形路由在障碍横挡直连走廊时，Z 形起始段可能穿过源节点框自身，退化直线兜底穿障（`route_avoiding` 红的根因，见 `src/graph/route/route.cpp`）。这不是 documented 局限，是未兑现契约——需多折点 / A* 栅格绕障 / 源框避让增强。增强后恢复 `route_avoiding` 的"不穿障"严格断言。
- [ ] **`metro` 直线兜底不穿框（documented 局限）**：`src/graph/layout/metro.cpp` 已注释声明直线兜底不保证不穿节点框。跟进实现增强（锚点候选对已修链边，跨链边/密集拓扑仍可能触底）后，逐步恢复 `route_metro` 严格断言（当前 `route_metro` 测试仅测链相邻边 + 严格断言，已验证绿，依赖 `metro.cpp` 锚点候选对修复）。
- [ ] **`dsl/d2.cpp:313-316`**：`ensure()` 超过 `max_nodes` 时 `report(OutOfRange)` 之后仍 `return ast_.ensure_node(key)`，逻辑歧义（虽最终 `run()` 因 `failed()` 丢弃结果）。报错后直接返回，不再 `ensure_node`。

---

## 收口待办（与体检无关，pending）

- [ ] **`route_metro` 严格测试（工作区未提交）与 `metro.cpp` 锚点候选对修复（你 modified 未提交）一起提交**——二者耦合，分开提交会在旧 metro.cpp 上变红。`aa5b33f` 已提交的 `route_metro` 放宽版会被工作区的严格版覆盖，最终取严格版。
- [ ] 顺手清理 `tests/unit/graph/_dbg_metro.cpp`（调试文件，确认无用后可删或加入 `.gitignore`，避免 GLOB 重扫纳入构建）。

---

### 建议执行顺序
1. **P0**（刷新文档数字，零风险，立刻做）
2. **P2 的"一行出图入口"**（最容易显著提升"好用"）
3. **P1**（接口统一需先拍板方向，影响面大）
4. **P3 / P4**（加分项与算法增强，按精力排）
