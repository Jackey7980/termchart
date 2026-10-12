# Changelog

本项目所有显著变更记录于此。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [SemVer](https://semver.org/lang/zh-CN/)。

> 本仓库尚未发布任何版本；`[Unreleased]` 以下条目是 2026-10-10 至今的开发进度
> 摘要，**权威口径**是 [`ROADMAP.md`](ROADMAP.md) 的「修订记录」——两处不一致时
> 以 ROADMAP 为准。

## [Unreleased]

### Added

- **阶段 0–1**（2026-10-10）：护栏三件套（分层 / 头文件自洽 / 生成物一致）、
  core / util / color / term / render / api 模块、终端上色闭环（example 01）。
- **阶段 2**（2026-10-11）：`scene` / `canvas` / `scale` / `raster` /
  `image`（Bitmap + 自包含 PNG 编解码）、盲文 2×4 与半块 1×2 子像素渲染
  （example 02）。
- **阶段 3**（2026-10-11 → 10-12）：`layout` / `chart` / `document` /
  三物种 `backend`（snapshot ansi·text·html_pre·json / vector svg /
  structured html·markdown）/ `interaction`（输入解析、命中测试、事件分发、
  焦点链）；带坐标轴的折线 / 柱状图（examples 03、04）；SVG / HTML / JSON
  导出（examples 34、35、36）；三个可扩展性契约实验（`tests/integration/replace_*.cpp`）。
- **阶段 4**（2026-10-11 → 10-13）：`graph` 模块（节点/边模型、分层与力导向等
  布局、正交 / 直线 / 样条 / 弧线路由、形状、占用栅格、标签避让）与 21 个
  `diagrams` 图种（flowchart、state_machine、mindmap、orgchart、uml_class、
  uml_sequence、er_diagram、metro_map、sankey 等）；示例 14–20；性能基线
  `bench/bench_sugiyama`、`bench/bench_force`。
- **阶段 5**（2026-10-12）：L17 `dsl`（lexer / parser_base / ast /
  dsl_registry / mermaid 系）与 mermaid 流程图解析（example 33）；
  `interaction` 深化（highlight / select / brush / legend_toggle / immediate，
  example 30）；`fuzz` 目标与 `ctest --preset fuzz`。
- **阶段 6 起步**（2026-10-11 → 10-12）：`image` 位图协议（sixel / kitty /
  iTerm2 内联 / 半块，example 38）。
- **chart 九个子目录铺开**（2026-10-12）：cartesian / polar / statistical /
  hierarchical / financial / flow / geo / text / indicator。
- **工程设施**：`.github/workflows/ci.yml`（护栏三件套 + ctest 双平台）、
  PNG 解码样本生成器、Turbo 色表官方数据源与生成链。

### Fixed

- Scene → Canvas 收口（R12）：描边改走 braille 2×4、填充走 halfblock 1×2，
  修复「盲文空白误判为空格导致多次收口互相擦除」的真 bug。
- metro 路由：45° 对角场景改用锚点候选对（16 对全枚举），消除穿框与退化。
- sankey 丝带 / 标签、knowledge_graph 力导向分离步长、uml_activity 的
  显式 initial/final 语义——已登记的 5 处已知红全部清零。
- Windows 控制台 VT 支持（`platform::enable_vt`）与真彩色探测。

### 已知边界（非缺陷，文档化）

- PNG 解码不支持 Adam7 隔行；16 位样本取高字节；
- `bench/layout_force` 为平方复杂度，大图需 Barnes-Hut；
- 能力矩阵与规格差距见 `docs/reference/capability_matrix.md`。
