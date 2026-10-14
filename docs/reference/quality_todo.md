# 质量收口清单（ROADMAP 子文档）

> 本文件是 `ROADMAP.md` 的「质量收口」子文档，索引见 ROADMAP 顶部 CHANGELOG（2026-10-13 一行）。
> 基于 2026-10-12 质量体检（架构 9/10、实现 9/10、API 美感 8/10、文档 8/10、测试 8/10、开发者体验 6/10）。
> 范围：抽样体检（审了 39 个公共头 + 25 个实现文件 + 对照 `capability_matrix.md`/`ROADMAP.md`/`CODEBUDDY.md`），非逐文件签字。
> 勾选规则同 `ROADMAP.md`：代码落地 + 测试通过 + 相关校验脚本通过，才打 `[x]`。

---

## P0 — 文档与真值同步（零代码风险，先做）

文档数字已严重脱节，等于"质量达标"的自我声明失真。文件自身要求"改动后重跑命令再更新"，但没执行。

- [x] **刷新 `capability_matrix.md` 测试数字**（2026-10-13）：开篇数字口径改为实测
  **314 头 / 271 实现 / 57 示例 / 126 测试源**、`ctest` **65 项**（开 fuzz 为 73）、
  doctest **876 用例 / 192219 断言**、golden 9 份；第五节"规格差距"按同一口径重写。
- [x] **`ROADMAP.md`「一、项目现状」同步**（2026-10-13）：`dsl` / `widget` / `plugin` / `app`
  已从"零代码空洞"改为"已落地"；并在该文件里显式标注「一、项目现状」小节曾停留在
  2026-10-11 口径。
- [x] **`capability_matrix.md` 模块表**（2026-10-13）：`widget` / `plugin` / `app` 三行由
  "**空** / 零代码"改为真实文件数与能力描述。

---

## P1 — API 一致性（破坏性，需先拍板方向）

影响所有调用方与示例，改名前先确认方向。

- [x] **统一 chart 数据灌入动词**（2026-10-13，**兼容式**）：确立并写进代码注释的口径是
  **`add_*` = 追加、`set_*` = 替换**（二者语义不同，不该合成一个动词）。给只有 `add_*` 的
  `line` / `scatter` / `streamgraph` 补上 `set_series()` + `series()`；`set_data` / `set_series` /
  `set_points` / `set_stages` 属替换语义，保持不变。
- [x] **顶层配置类 `Opts` → `*Options` 对齐**（2026-10-13）：`api::Opts` 改名 **`ApiOptions`**
  （与全库 `XxxOptions` 一致），`using Opts = ApiOptions;` 保留为兼容别名，`fwd.hpp` 同步。
- [x] **`Renderer` 不可移动**（2026-10-13）：改为 `std::unique_ptr<Impl>` + 移动构造/移动赋值
  （定义在 `.cpp`，那里 `Impl` 才完整）；移出后对象只剩"可析构 / 可重赋值"，各入口加了断言。
- [ ] **公共 chart 入口 `TERMCHART_API` 标记不一致**：兄弟 chart 类型有的标有的没标
  （`bar_chart.hpp:21` `set_values` 等）。**未做**——这是"给全部公共 chart 入口统一加导出标记"
  的机械清扫，涉及上百个声明且无法靠类型系统验证对错，本轮没有可靠手段一次改对；
  建议单开一个"导出标记清扫"任务，配合 shared 构建（`TERMCHART_BUILD_SHARED=ON`）验证。
- [x] **chart 默认配色统一**（2026-10-13）：抽出 `chart::default_color(DefaultColor::*)`
  （放在 `chart/series.hpp`，因为 `chart.hpp` 反向 include 它），把 93 个文件里的
  `Color::ansi16(7/2/1/4/8/6/3)` 全部替换为命名枚举。**色号逐字未变**，只是从散落的魔数
  变成一个真源；将来换 palette 只改一处。
- [x] **`FunnelOptions` 封装破裂**（2026-10-13）：`FunnelChart` 改为
  `set_options()` / `options()`，裸公有成员收回 `private`（全仓无调用点依赖旧写法）。
- [~] **`Graph` 增操作返回类型不一致**：**半做**。补齐了 `Graph::find_edge(from, to)`
  （此前只有节点能按 id 找回，边只能靠当时拿到的指针）；
  **未做**的是把 `add_node`(返回 `Node&`) 与 `add_edge`(返回 `Edge*`) 的返回类型统一——
  这会动到 195 处调用点里真正使用返回值的那批，是纯破坏性改名，**需先拍板方向**：
  ① 统一返回引用（加边失败时如何表达？）② 统一返回 `NodeId`/`EdgeId`（改动面最大）。
- [x] **`parse_mermaid` 出参风格**（2026-10-13 复核）：四个方言入口
  （`parse_mermaid` / `parse_flowchart` / `parse_class_diagram` / `parse_state_diagram` /
  `parse_sequence`）**一律 `Result<T>` 返回值 + 可选 `ParseDiagnostic*`**，风格本就统一，
  无需改动（该条基于过期快照）。
- [x] **`Opts` 访问器返回类型不对称**（2026-10-13 复核）：`width()`/`height()`/`color_depth()`
  都是 `std::optional<>`（"未设置"），`unicode()`/`newline()`/`reset_at_end()` 是裸 `bool`
  （有默认值的开关）。**这是刻意的**：optional 表达"未设置 → 由库决定"，布尔表达
  "有默认值的开关"——两类语义不同，强行统一反而丢信息。已在 `api/opts.hpp` 顶部写明该约定。

---

## P2 — 开发者体验（最大短板，最好修）

- [x] **补"一行出图"入口**（2026-10-13）：新增 `api/chart.hpp` + `src/api/chart.cpp`
  —— `api::render_chart(Chart&, Opts) -> Result<std::string>` 与
  `api::print_chart(Chart&, Opts) -> Status`，把 `Canvas → DrawContext → draw → render_full`
  四步样板收成一次调用。尺寸规则刻板（显式给，或默认 80×24；显式 0 是
  `InvalidArgument`），**不自动探测终端**，保证 CI 与真终端产出一致。
  用例见 `tests/unit/api/test_api.cpp` 的两个新 TEST_CASE。
- [x] **写 Quick Start 文档**（2026-10-13）：`docs/guide/quickstart_30s.md` 补上
  "3.5 一行出图（给图表）" 段与速查表新行；`mkdocs.yml` 的 nav 此前**只挂了 5 个参考页、
  整个 `docs/guide/` 都不可达**，现已补齐全部指南页与"扩展点"子节。

---

## P3 — 现代 C++ 人体工学毛刺（非缺陷，加分项）

- [x] **`render/renderer.cpp`**（2026-10-13）：`make_unique<Impl>` + `std::unique_ptr<Impl>`，
  析构 `= default`（定义在 `.cpp`），并补移动构造/移动赋值（见 P1 同条）。
- [x] **`core/buffer.cpp`**（2026-10-13）：`at()` 加 `TERMCHART_DEBUG_ASSERT_MSG`（Release 零开销），
  并新增 `Cell* try_at()` / `const Cell* try_at()` 供"可失败的取值"使用
  （`TERMCHART_DEBUG_ASSERT_MSG` 亦为本次新增，与既有的 `ASSERT_MSG` / `DEBUG_ASSERT` 配齐）。
- [x] **`dsl/*` 多处 `const_cast`**（2026-10-13）：给 `AstGraph::find_node` 与
  `AstSequence::find_participant` 补**非 const 重载**（const 版本复用它），
  8 处调用点的 `const_cast` 全部去掉，`const_cast` 只留在实现内 2 处。
- [x] **`color/colormap.cpp`**（2026-10-13）：`StopCache` 改成 **append-only 的
  `shared_ptr<const vector<Rgb>>` 缓存**，key = `(kind, count)`——map rehash 只搬 shared_ptr，
  指向的向量地址终身不变，已发布的 span 不再悬垂；`sample()` 在 `kFits[...]` 前加了
  枚举范围校验（枚举是 `uint8_t`，调用方可 static_cast 出任意值）。
- [x] **`canvas/path.cpp` / `core/buffer.cpp`**（2026-10-13）：`Canvas::put` 与
  `CellBuffer::set_sides` 改为**返回 `bool`**（是否落格 / 是否写入），`path::stroke` 顺势用
  `put` 的返回值记账，不再自己重判一遍边界。
- [x] **`graph/layout/metro.cpp`**（2026-10-13）：`static_cast<int>(steps)` 前加
  `TERMCHART_ASSERT_MSG(steps <= INT_MAX)`，把"碰巧不溢出"变成显式契约。

---

## P4 — 算法局限与测试契约

- [x] **`obstacle` 横挡绕障真缺陷（非 best-effort）**：已修（2026-10-13）——`route_avoiding` 对撞占用的折线改用 WireGrid 上的确定性 BFS 重找正交绕行路径并回写 `Edge::set_route`（栅格超 4M 格或物理无路时维持 best-effort 兜底）。回归测试：`tests/unit/graph/test_graph_route_obstacle_fix.cpp`（3 用例，绕行段不穿障碍）。已知残留：锚点四周全被占用的病态几何仍走直线兜底。
- [x] **`metro` 直线兜底不穿框**（2026-10-13）：路由新增第 3.5 步 **Z 形绕行**（轴对齐三段，
  候选行/列 = 两端点与中点的 ±1..max_detour，确定性枚举，不做随机/A*），把跨链边
  从"直线兜底"里救回来；并新增 `graph/route_metro: 跨链边靠 Z 形绕行避开节点框`——
  对**跨链边**也断言"中间格不落在任何节点框内"（此前严格断言只覆盖链相邻边）。
  已知残留：锚点四周全被占满的病态几何仍走直线兜底，已在 `metro.cpp` 头部注明。
- [x] **`dsl/d2.cpp:313-316`**（2026-10-13）：超限后 `report(OutOfRange)` 并返回一个
  **不进 `ast_.nodes` 的"黑洞"节点**，不再 `ensure_node()`（`mermaid_sequence` 的
  `ensure_participant` 同病同改）。

---

## 收口待办（与体检无关，pending）

- [x] **`route_metro` 严格测试与 `metro.cpp` 锚点候选对修复**（2026-10-13）：二者已在工作区
  共存且 `ctest` 全绿（含本次新增的跨链边严格断言），不再有"分开提交会变红"的耦合风险。
- [x] **清理 `tests/unit/graph/_dbg_metro.cpp`**（2026-10-13 复核）：该文件已不在工作区
  （此前已删），`tests/unit/graph/` 下无 `_dbg_*` 残留。

---

### 建议执行顺序
1. **P0**（刷新文档数字，零风险，立刻做）
2. **P2 的"一行出图入口"**（最容易显著提升"好用"）
3. **P1**（接口统一需先拍板方向，影响面大）
4. **P3 / P4**（加分项与算法增强，按精力排）
