# Changelog

本项目所有显著变更记录于此。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [SemVer](https://semver.org/lang/zh-CN/)。

> 版本号与 [`ROADMAP.md`](ROADMAP.md) 的「修订记录」同源，两处不一致时以 ROADMAP
> 为准；某个 tag 的具体内容也可直接 `git show <tag>` 查看（`v0.8.0` / `v0.8.1` 的
> 说明写在 tag 上）。

## [Unreleased]

### Fixed

- **Sugiyama 示例的位图/矢量出口不画箭头**（方向信息全丢）：终端路径有 `draw_arrows()`，
  而 `build_scene()`（PNG/SVG 出口）只描折线、从不画箭头头 ⇒ 分层拓扑图在位图里看不出
  边指向。现按 `Edge::arrow()`（`None` / `AtTarget` / `Both`）在 route 末端/起点画
  "V"形箭头（场景单位 1 格长、±24°）。

## [1.0.0] - 2026-11-01

首个语义化稳定版：核心稳定、可用、值得信任。本版把"图像/视频编码、DSL 解析、Python 绑定、
主题体系"四块补齐到可发布状态，并修掉若干真实缺陷（含 2 个会让产物静默降级/崩溃的 bug）。

### Breaking

- **`termchart::image::ImageFormat`（解码侧）更名为 `DetectedFormat`**：此前 `image` 命名空间下
  同时存在两个 `ImageFormat`（编码侧的导出格式集 / 解码侧的嗅探格式集），底层类型还不一致。
  普通构建没有 TU 同时 include 两者所以不报错，但**单头合并产物**会把两者拼进同一 TU，直接
  "multiple definition" 编译失败。解码侧重命名为 `DetectedFormat`；编码侧 `ImageFormat`
  （`encode()` / `save_image()` 的公共 API）**保持不变**。

### Added

- **图表统一 `to_scene` 大迁移 + GeoMap 收尾**：`chart/` 各图种统一产出 `scene::Scene` IR（cartesian 核心 7 + 剩余 14、indicator 5、text 4、statistical 11、flow 2、polar 8、geo 5、hierarchical 6、financial 6 等，共 73 个图表头已接 `to_scene`）；`chart/geo`（GeoMap）补齐分级图例/色标、命中测试、离线地图数据分发、TopoJSON 缝合离线回归；`backend` 加 SVG/PDF 文字轮廓化 `text-to-outline`。

- **自包含 VP8L 无损 WebP 编码器**（零外部依赖）：`image/webp_encode` 按 WebP Lossless
  Bitstream 规范实现（RIFF/WEBP/VP8L 容器 + 5 棵 canonical Huffman），接进统一分发器
  `encode(ImageFormat::Webp)`；示例 `85_webp_lossless` + 单测（9 用例 / 91 断言，含独立最小
  VP8L 解码器逐字节回环）。
- **自包含 VP8L WebP 解码器**：WebP 读取不再强制依赖可选的 stb_image。
- **H.264 / H.265 自写编码器 + MP4 封装**：视频侧零外部依赖（不调 libavformat/ffmpeg）。
- **GIF 多帧 + 循环**：NETSCAPE2.0 loop 扩展、逐帧延时、median-cut 调色板 + LZW。
- **DSL 解析补齐**：Mermaid Sequence 的 `loop`/`alt`/`else` 等逻辑块（标签、嵌套深度、分支、
  每条消息的归属全部进 AST）；Graphviz DOT 的 `node [...]` / `edge [...]` 全局默认属性
  （对所有节点/边生效，显式属性优先）；D2 的 `style: {...}` / `vars:` / `classes:` 块
  （不再整块丢弃，也不再造出名为 `classes` 的幽灵节点）；PlantUML 类成员的可见性 / 名称 /
  参数表 / 返回类型结构化（原文字段保持原样原序）。
- **AST 查找 O(1)**：`AstGraph` 增加 key→下标索引（`nodes` 仍是唯一真源，索引只是加速器，
  外部直接改 `nodes` 时自动重建）。
- **CSV 流式解析**：`parse_csv_stream(istream&, …, CsvRowSink&)` 内存 O(一行)；`sniff_columns`
  改为只读样本行推断类型，不再为推断 100 行把整个文件读进内存。
- **Python 绑定可装可导入**：`cmake.source-dir` 修正为仓库根、补 `install(TARGETS …)` 与
  `.pyi` 存根安装（此前模块能编出来但进不了 wheel，`pip install` 后直接 ImportError）；
  wheel 不再背上 200MB 的 C++ SDK；类型存根与模块实际导出完全一致。
- **全局主题打通**：调色板下沉到 `color/`（L2）——此前它住在 `widget/`（L18），而分层规则
  "只能 include 层号 ≤ 自己模块"使得 `chart`（L12）根本引用不到，"全局主题"实际分裂成两套
  配色。现 `chart` 与 `widget` 共用一份色板，`chart::set_default_palette()` 装一张即全局换肤
  （缺省仍走历史 ansi16 色号，图表金标输出零变化）。
- **CI 认证**：新增 `encoder-cert` 作业，用真实解码器（Pillow / ffprobe）校验 WebP 产出，
  不再只靠库内自测。

### Fixed

- **`VideoCodec::AV1` 静默产出 H.264 的 MP4**（真 bug）：旧实现用 `is_hevc = (codec == H265)`
  二分，AV1 落进 H.264 分支——调用方请求 AV1，拿到的是货真价实的 H.264 文件且不报错。
  现在 `is_video_codec_supported()` 可预检，`encode_mp4` 对未实现码流显式失败，
  `save_image` 返回可区分的 `ErrorCode::Unsupported`。
- **CSV 流式解析的无限循环**（真 bug）：`read_field_stream` 读到分隔符只 peek 不消费，而
  `read_row_stream` 未跳过它 → 下一轮又 peek 到同一分隔符 → 空字段 → 又是 Delimiter →
  死循环，`fields` 无限增长直至 `std::bad_alloc`。任何含分隔符的输入都会触发。
- **全量编译告警归零**：`-Wall -Wextra -Wshadow -Wconversion` 下 13 处 sign-conversion、
  `-Wold-style-cast`、`-Woverloaded-virtual`（`BoundsVisitor`/`CountingVisitor` 只 override
  部分 `visit`，隐藏了基类其余 14 个重载）、`-Wmissing-field-initializers`、
  `-Wformat-nonliteral` 全部清零（clean 全量重编 0 warning / 0 error）。
- **guardrails 两红**：`check_header_only` 的"缺失同名实现 + 孤儿实现"双重违规（`mp4_mux.cpp`
  更名 `mp4.cpp` 与 `mp4.hpp` 配对）、WASM 入口 TU 走白名单豁免；`gen_single_header --check`
  由不一致转一致（单头产物按全部头改动重生成）。
- **PDF 后端**：未设色时原先写出非法的 `"rg g"`，改为落黑色；路径按描边色画线、不填充，与
  SVG / `scene_renderer` 约定一致。
- **GUI 渲染库 `ChartGeom` 不变式越界崩溃**：`termchart_gui` 的 `lerp_geom`→`recompute_tangents` 在动画收敛时对 `g.xs`/`g.ly` 越界下标（`vector<double>::operator[]` abort）。根因 `GaugeRenderer`/`PolarRenderer::compute_geom` 只设 `n=1` 却留空 `xs`/`ly`，破坏“长度 == n”不变式，且 `lerp_geom` 守卫漏校验 `ly.size()` 与 `xs.size()==n`；天气 demo 联网必崩。修复：`recompute_tangents` 按 `min(n, xs, ly)` 收敛、`lerp_geom` 强制不变式校验（不齐即吸附目标）、`gauge`/`polar` 的 `compute_geom` 补齐 `xs`/`ly` 到 `n`；新增 `--dump`/`--simulate`/`--feed` 诊断开关；gdb 复现并验证真实联网交互不崩。

## [0.9.0] - 2026-10-24

本版把 AutoViz 对**不可信输入**的处理口径收口（`v0.8.1` 之后的四类修复见下面
「Fixed」第一条），不含新的图表类型。下列「阶段」条目是 v0.1–v0.8 的能力汇总——
它们随首个 `v0.8.0` tag 已经发布，此处一并留档便于查阅。


- **AutoViz（L21）**（2026-10-16 → 10-24）：`api::auto_plot` 一行出图——列画像
  （数值 / 时间 / 类别 / 文本、量纲标记、桶宽推断）→ 候选管道（时间折线、类别折线、
  类别柱状、事件柱状、直方图）→ 解析分 + 光栅分 + 否决 → 选中出图；离群值裁剪双闸
  （小样本保护）、日历对齐的 X 刻度、`notes` 留痕、`final_clip_desc` 单一播报口径。
  用法见 `docs/guide/autoviz.md`，边界见 `docs/limits.md`。
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

- **AutoViz 容错语义（v0.9.0）**：裁剪结论双出口（图题写"裁去 2 个极值"、告警写
  "不裁轴"）收敛到 `ChartCandidate::final_clip_desc` 单一真源；X 轴假精度（真凶是
  `scale::ticks_for` 在 epoch 秒上按 10 的幂取整成 50000s，把 41 小时的数据撑到
  02-28 17:33 ~ 03-03 01:06）改走 `nice_bucket_seconds` 的日历对齐档位；JSONL 位置式
  数组行按列序 zip 并计 `lines_zipped`（宽度不符仍丢弃计 `lines_skipped`）；`15%` 与
  金额混列按缺失处理并告警（**拒绝 ×100** —— 那是把 15% 伪造成 15 元），空值率不再对
  量纲闸双重记账（否则 1 个格子就能把折线解析分从 0.968 压到 0.918、输给事件柱状图）。
- Scene → Canvas 收口（R12）：描边改走 braille 2×4、填充走 halfblock 1×2，
  修复「盲文空白误判为空格导致多次收口互相擦除」的真 bug。
- metro 路由：45° 对角场景改用锚点候选对（16 对全枚举），消除穿框与退化。
- sankey 丝带 / 标签、knowledge_graph 力导向分离步长、uml_activity 的
  显式 initial/final 语义——已登记的 5 处已知红全部清零。
- Windows 控制台 VT 支持（`platform::enable_vt`）与真彩色探测。

### 已知边界（非缺陷，文档化）

- PNG 解码不支持 Adam7 隔行；16 位样本取高字节；
- `bench/layout_force` 为平方复杂度，大图需 Barnes-Hut；
- 位置式数组、百分比混列、epoch 数字串、时间格式族与时区口径详见
  [`docs/limits.md`](docs/limits.md)；
- 能力矩阵与规格差距见 `docs/reference/capability_matrix.md`。
