# 能力矩阵（手写，非生成物）

本文件回答三个问题：**现在真正能做什么**、**哪些路径仍是占位**、**规格里还差什么**。
它由人手维护；`docs/reference/module_matrix.md` 是生成物（`tools/gen_module_matrix.py`），
两者不要互相抄。

- 规格（做成什么样）：`结构.md`
- 计划（什么时候做）：`ROADMAP.md`
- 本文件（当前真实能力与缺口）：这里

数字口径（2026-10-13 实测）：`include/termchart/**/*.hpp`（310 个）、`src/**/*.cpp`（271 个）、
`examples/`（57 个源，规格示例 01–54 齐备）、`tests/`（126 个源）、golden 9 份。
`ctest` **65 项**（5 套件 + 57 示例 + 3 bench 冒烟；开 fuzz 后为 73 项，
即 `ctest --preset fuzz` 另加 8 条 fuzz 自检），doctest **900 用例 / 192306 断言**。
改动后请重跑下列命令再更新本文件。

```bash
cmake --build --preset ninja
build/ninja/tests/termchart_tests.exe          # 单元 + 集成 + golden
ctest --preset ninja                            # 套件 + 示例 + bench 冒烟
cmake --build --preset ninja --target check_all # 分层 + 自洽 + 生成物 + 每头独立编译
```

---

## 一、模块能力（L0–L21）

| 模块 | 层 | 状态 | 真实能力 | 占位 / 缺口 |
| --- | --- | --- | --- | --- |
| `core` | 0 | 可用 | 单元格/缓冲/样式/属性/点矩形/错误码 | — |
| `util` | 1 | 可用 | UTF-8、grapheme、宽字符、文本、格式化、日志、**几何**（点线距离/严格线段相交/多边形/凸包）、**计时**（单调时钟/秒表/帧计时/超时）、**调度**（单线程定时任务，时间源可注入）、**可取消任务句柄**、**base64** | — （`base64` 是规格清单外的补充，理由见 ROADMAP 的 `util/` 条目） |
| `color` | 2 | 可用 | xterm-256/16 色表、量化、调色板、渐变、**`resolve`（Color/Style → 真正 RGB，GUI 与位图输出的分界点）**、**`colormap` 的 Turbo 用官方 256 级色表**（数据源 `assets/colormaps/turbo.dat` → `tools/gen_turbo_table.py` → `detail/turbo_table.hpp`；级点处与官方表逐字节一致） | — |
| `detail` | −1 | 生成物 | 盲文表、块字符、Unicode 属性表、**Turbo 色表** | 层号 −1（横切数据表，任何层可 include）；由 `tools/` 生成，**禁止手改**；`turbo.dat` 的取数脚本需要 matplotlib，故不进构建管线（`gen_turbo_table.py` 才是管线里的那一环） |
| `term` | 7 | 可用 | ANSI 序列、能力探测、光标、平台拆分、**鼠标开关**（模式/编码两组 DECSET）、**OSC 52 剪贴板**、**OSC 8 超链接**、**DA1/DA2 主动探测与回复解析**、**颜色降级桥接** | — （规格 13 个文件全部落地） |
| `render` | 14 | 可用 | 全量/增量渲染、差分、SGR 序列化（唯一生成点）、脏区 | `compositor` 未实现 |
| `api` | 21 | 可用 | `print`、`quick`（一行式）、状态码 | 规格的 `quick_line` `quick_bar` `quick_spark` `quick_table` 未实现 |
| `scene` | 4 | 可用 | 矢量 IR（命令变体 + 访问者 + SceneBuilder）、**`bounds`（几何包围盒，GUI 决定画布/无效区时需要）** | — |
| `canvas` | 10 | 可用 | 画布、图元、`Scene → Canvas` 桥接、**扫描线填充**（横向精确覆盖 + 纵向超采样）、**路径**（曲线自适应细分、四段贝塞尔圆）、**图层合成**（Overlay/Replace/Tint + 最小重绘包围盒）、**2D 仿射变换**（含字符宽高比补偿）、**裁剪**（矩形精确 / 凸多边形 / 非凸明确不支持多边形裁剪） | `ImageOp` 仍是阴影块占位（Scene 无像素源） |
| `scale` | 3 | 可用 | 线性/对数/nice/刻度/自动域 + **注册表** | 规格的 `time` `category` `band` 未实现 |
| `image` | 5 | 可用 | `Bitmap`（RGBA，含 `fill_rect`/`blit_over`/`scaled_to`/通道转换等合成原语）、`FloatField`（标量场）、`convert`（位图 ↔ 标量场：Rec.601 亮度、阈值双色）、`encode_png`（自包含 PNG 编码）、**`png` 解码**（自带完整 inflate：stored/固定/动态 Huffman + 五种滤波 + 颜色类型 0/2/3/4/6 + 1/2/4/8/16 位深 + tRNS）、**`decoder`**（按魔数识别格式并分发） | 不支持 Adam7 隔行（报 `Unsupported`）；16 位取样到高字节；仅 PNG 有解码器（JPEG/GIF/BMP/WebP 报 `Unsupported`） |
| `raster` | 6 | 可用 | 盲文/半块/象限/竖条/密度字符 + 注册表 + 自动选择 | `sextant`（U+1FBxx 新区块，终端支持度低）显式推迟 |
| `layout` | 11 | 可用 | `PlotArea`、轴/网格/图例/标题/标签、**`facet`（纯几何矩形划分，不认识 chart）**、**`auto_margin`（由标签宽度/标题/图例反推四边留白）**、**`label_avoid`（通用避让，graph 侧转引同一实现）**、**`annotation`（参考线/区域高亮/标注，一律裁剪在绘图区内）** | — （规格 11 个文件全部落地） |
| `chart` | 12 | **可用（9 子目录规格齐备，68 文件）** | 基类 + 系列 + 注册表 + `context`（绘制开关）+ `dataset`；**cartesian 23/23**（在既有 7 个之上补齐 bubble/candlestick/boxplot/violin/contour/errorbar/waterfall/gantt/dumbbell/ridgeline/parallel/mosaic/qq/bullet/streamgraph）；**polar 8/8**（pie/donut/rose/radar/gauge/polar_bar/polar_line/polar_heat）；**statistical 11/11**（distribution/ecdf/corr/pca/regression/roc/forest/survival/autocorr/seasonal/bland_altman）；**hierarchical 6+1**（treemap/sunburst/dendrogram/pack/icicle/tree_view + node 辅助）；**financial 7/7**（depth/volume/renko/kagi/point_figure/yield_curve/drawdown）；**flow 2/2**（funnel/stream）；**geo 4/4**（choropleth/tile_grid/hexbin/dot_density）；**text 4/4**（wordcloud/tag_cloud/table/progress）；**indicator 5/5**（gauge_kpi/progress_ring/star_rating/status_light/thermometer）。测试：九个子目录测试文件全部接入（ctest 全绿，739 用例 / 190594 断言） | 各图种多系列/堆叠变体未做 |
| `document` | 8 | 可用 | Document/Block/Span/TableModel IR | — |
| `graph` | 13 | 部分 | 节点/边/元数据/占用栅格/箭头/端口/标签避让/交叉计数（判定复用 `util/geom`）；布局（网格/分层/根树/力导向/**径向/圆周**——径向按子树扇区均分、半径按层递增，圆周按外接圆判据推半径保证不重叠；**`metro` 地铁 45°（链分解 + 之字 + 锚点候选对栅格路由）**/ **`cluster` 聚类**）；路由（直线/正交/**样条 `spline`**/**弧线 `arc`**/**`bundling` 边捆绑**/**`obstacle` 障碍避让**）；**形状八种**；图种（流程图/状态机图/思维导图/**组织架构图**/节点-连线/**`dag`（含环检测）**/**`dependency`**） | `sugiyama`（分层已覆盖其分层部分）`kamada_kawai`；`18_uml_sequence` 示例（等序列渲染） |
| `interaction` | 15 | **可用** | 输入解析（ANSI/UTF-8/SGR 鼠标流式，未完成序列有 **256 KiB 缓冲上限**）、事件、命中测试、分发、焦点、**视口缩放/平移**（`Viewport`：锚点缩放 + 双向夹取；`PanGesture`：纯几何拖拽状态机）、**悬停读数层 `crosshair`**（横/纵准线 + 轴侧读数，默认只改样式保留数据字符）、**浮层 `tooltip`**（边框 + 自动翻边 + 宽字符按 2 列算 + 超长截断）；后两者的核心不变量都是**`clear()` 逐格逐属性精确还原**（覆盖层不得永久污染画布）；**`highlight`**（高亮放大：锚点居中、跨度 /magnify）、**`select`**（框选状态机 begin/update/end/cancel，矩形归一化 + payloads_inside 保序）、**`brush`**（半径吸附刷选，首次顺序去重）、**`legend_toggle`**（id→可见性，payload 0 永不生效，click_at 复用 topmost_hit）、**`immediate`**（Event 流→Viewport/BoxSelect 的路由粘合层）。各配单测；`examples/30` 锁图例语义 | 序列图/表格的交互绑定（等渲染侧落地） |
| `backend` | 16 | 可用 | 三物种各至少一个后端：snapshot（ansi/text/html_pre/json）、vector（svg）、structured（html/markdown） | `png_screenshot`；structured/vector 的 `latex_backend` |
| `protocol` | 9 | 可用 | sixel（量化+调色板+RLE）、kitty（原始 RGBA+base64 分块）、**iterm2 内联（PNG+base64+OSC 1337）**、半块图像、降级选择（Kitty > Iterm2 > Sixel > HalfBlock > None）、环境探测（含 WezTerm/Konsole/iTerm 与手动覆盖） | — |
| `dsl` | 17 | **可用（14/14 文件齐备）** | 词法/框架：`lexer`（箭头感知切分 / UTF-8 标签 / 注释前缀可配 / 非法字节不阻塞游标）、`parser_base`（递归下降 + 首个诊断定位）、`ast`（+`to_graph` 唯一适配点）、`dsl_registry`（function-local static + 快照语义 + 幂等内置登记，登记 6 个图产出方言）。方言：`mermaid_flow`（8 形状 / 9 箭头 / 两种边标签 / 边链 / subgraph）、`mermaid_class`（类体成员 + 六种关系语义 + 多重度 + 构造型）、`mermaid_state`（`[*]` 起止 + 组合态 + note）、`mermaid_sequence`（**独立类型 `AstSequence`**：参与者 / 8 种消息 / note / 激活，刻意不进图模型）、`graphviz_dot`（rankdir / 属性按原文切片 / 并列节点集 / `{a b}->c` 展开）、`plantuml`（类体 + 五关系 + 多重度 + 宽松容错）、`d2`（`name: Type` 形状 / 连接 / 容器块 / `name.prop` 属性）、`table_markdown`（GFM 管道表格 + 转义竖线）、`csv_sniff`（引号字段跨行 / BOM / 七类列型推断）。测试：`tests/unit/dsl/` 5 个文件、`tests/fixtures/` 4 份生成物、`fuzz_mermaid` + `fuzz_dsl_lexer`（`--self-test` 进 ctest）、`bench_mermaid_parse`（200/400/800/1600 节点四方言基线） | `table_markdown`/`csv_sniff`/`mermaid_sequence` 的**渲染**侧（尚无表格图与时序图渲染器）；DOT 的 HTML 标签与端口、PlantUML 的组件/活动图族、D2 的 SQL/markdown 嵌入块 |
| `widget` | 18 | **可用（6 头 / 3 实现）** | `widget`（布局 + 生命周期 + 事件注入）、`immediate_widget`（每帧重建的即时模式）、`reactive`（信号式，脏标记 + 重建）、主题三份（`theme_dark` / `theme_light` / `theme_mono`） | GUI 平行栈的 `widget` 层由 GUI 自己重写（设计选择，见第六节） |
| `plugin` | 19 | **可用（4 头 / 4 实现）** | `plugin`（接口 + 生命周期）、`manifest`（元数据）、`loader`（加载）、`dynamic`（动态库）；注册表为 function-local static + App 构造时冻结快照（见 `docs/design/registry.md`） | 真·跨动态库加载（当前为同进程登记） |
| `app` | 20 | **可用（7 头 / 7 实现）** | `app`（reactive 主循环）/ `immediate_app`（即时模式循环）、`loop`（事件循环）、`headless`（无 tty 可测）、`animation`（帧调度）、`post`（帧后处理）、`config`（运行期配置） | 平台原生循环（GUI 侧自写） |
| `gui` | 22 | **可用（3 头 / 3 实现）** | `font`（5×7 点阵等宽字体：ASCII 全量 95 字形、缺字画占位框、UTF-8/显示宽度口径）、`scene_renderer`（Scene → RGBA Bitmap：3× 超采样盒式 AA、线宽随 autofit 与 SVG 同构、文本下采样后直绘置顶、透明背景）、`hover_chart`（悬停层：`chart::HitRegion` + `interaction::topmost_hit` 命中 → 浮层翻译成 RectOp/TextOp 叠进 Scene 副本 → 重放；渲染器零改动。`examples/55` 出底图、`examples/58` 出悬停帧） | 文本仅内置 ASCII 字形（非 ASCII 画占位框）；真实窗口/鼠标事件循环在平行栈 `termchart_gui`（`backend_tk` 现为无头参考） |
| `termchart_gui` | — | **平行栈骨架已落地（见 `include/termchart_gui/`）** | 自写 `layout`/`widget_tree`/`native_loop`/`backend_tk`/`render::style_map`；呈现侧复用主库 `gui/font` + `gui/scene_renderer` + `gui/hover_chart` | 真实 tk 后端未接（现为 `HeadlessBackend` + `BitmapCanvas`）；悬停命中链路已在主库 `gui` 侧打通 |

## 二、三物种导出能力

| 物种 | 输入 | 后端 | 输出 |
| --- | --- | --- | --- |
| snapshot | `CellBuffer` | `ansi` / `text` / `html_pre` / `json` | 字节流（ANSI 含 SGR，复用 `render/sgr` 唯一生成点） |
| vector | `scene::Scene` | `svg` | `<svg>`（路径/圆弧/文本/图片占位） |
| structured | `document::Document` | `html` / `markdown` | 完整 HTML 文档 / Markdown |

> 后端不做跨渲染模型转换：输入是哪一种物种，就在类型上钉死（见各 `backend/*/backend.hpp`）。

## 三、示例索引（57 个源，规格示例 01–54 齐备，全部在 ctest 里冒烟）

| 示例 | 路径 | 渲染方式 | 备注 |
| --- | --- | --- | --- |
| `01_hello_color` | 终端 | `render` + SGR | 颜色握手 |
| `02_sine_wave` | 终端 | 直连 `raster`（盲文/密度） | 子像素正弦波 |
| `03_function_plot` / `03_line_chart` | 终端 | `canvas` + 显式字符 + `raster` 直连 | 带坐标轴的折线 |
| `04_bar_chart` / `04_heatmap` | 终端 | 同上 | 柱状 / 热力 |
| `05_showcase` | 终端 | 直连 `raster`（盲文 + 半块双面板） | 真彩旗舰展示 |
| `06_dashboard` | 终端 | 直连 `canvas` + `raster` | 仪表盘拼版 |
| `07_fractal` | 终端 | 直连 `raster` + `colormap` | Mandelbrot 分形；`--depth` / `--raster` 可产出降级对比 |
| `08_fractal_levels` | 终端 | 同上 | 2×2 四连拍：真彩 / 256 / 16 / ASCII 同屏 |
| `14_flowchart` | 终端 | `graph/diagrams` → `canvas` | 分层布局 + 正交路由 |
| `15_state_machine` | 终端 | 同上 + 转移标签 | 起止标记 |
| `16_mindmap` | 终端 | 树布局 + 扇形连线 | 分支两侧分列 |
| `17_uml_class` | 终端 | `graph` 模型 + 分层布局 + **自绘三栏类框** | UML 类图：`◁` 空心三角泛化箭头 + 总线接点收口（不吃 diagrams 骨架，因为框内要放三栏内容） |
| `20_force_graph` | 终端 | 力导向 + 节点-连线视图 | 性能见 `bench_force` |
| `19_metro_map` | 终端 | `graph/route/junction` + `layout::place_labels` | 示意式线路图（环线 + 横线 + 竖线 + 2:1 斜线，13 站 9 换乘）：交汇口用**公共规则**、站名批量交给**公共避让**；标题统计与正文同源计算（不硬编码） |
| `21_hover_tooltip` | 终端 | `interaction::Tooltip` + `Crosshair` | 悬停读数：准线（背景高亮、保留数据字符）+ 浮层（自动翻边、宽字符按 2 列、超长截断）；**末尾自带对照帧**——`clear()` 后与"从未悬停"逐格一致，不一致即退出码 1 |
| `22_zoom_pan` | 终端 | `interaction::Viewport` + `PanGesture` | 滚轮缩放（锚点不动）+ 拖拽平移（内容跟随指针、贴边夹取）；**脚本/交互两模式共用同一条事件链**，故能进 ctest 冒烟 |
| `32_quick_one_liner` | 终端 | `api/quick` | 一行式 API |
| `34_export_svg` | 文件 | `scene::Scene` → `vector/svg` | 同一份 Scene 亦可用 `render_scene` 出终端图 |
| `35_export_html` | 文件 | `document` → `structured/html` | |
| `36_export_json` | 文件 | `CellBuffer` → `snapshot/json` | 逐格 cp/颜色/属性 |
| `38_image_sixel` | 终端 | `protocol`（kitty / iterm2 / sixel / 半块自动降级） | 本机无图像协议时输出提示；`TERMCHART_ITERM2=1` 可强制走 iTerm2 内联 |
| `40_advanced_master` | 终端 | `canvas` + `raster` + `colormap` | 旗舰展示：盲文折线 / Turbo 渐变柱 / 真彩热力图同屏（并行会话新增） |

**重要澄清（避免再踩 R12 的坑）**：`chart`/`graph`/`layout` 这些"高级 API"示例
**本来就直连 `canvas.draw_line`/`draw_text` + 显式字符**，从不经过 `scene::Scene`，
因此历史上从未出现 `*` 占位（实测占位计数为 0）。R12 的真实影响面是
**同一份 `scene::Scene` 在终端侧能否出真图**——该缝已收口：`render_scene` 的描边
走 braille 2×4 子像素、填充走 halfblock 1×2 实心（golden `scene_raster/basic.txt` 锁外观）。

## 四、性能基线（`bench/`，零外部依赖）

```bash
build/ninja/bench/bench_sugiyama          # 分层布局扩展性
build/ninja/bench/bench_force             # 力导向收敛
build/ninja/bench/bench_<name> --quick    # ctest 冒烟档
```

| bench | 规模 | 中位耗时 | 结论 |
| --- | --- | --- | --- |
| `layout_layered` | 64 节点 / 105 边 | 120 µs | 64→1024 节点耗时 ×18 ≈ 线性，符合 O(V+E) |
| `layout_layered` | 1024 节点 / 1953 边 | 2210 µs | 无平方级行为 |
| `layout_force` | 20 节点 / 200 迭代 | 2.6 ms | 20→160 节点耗时 ×56 ≈ 平方，符合每轮 O(V²) 斥力 |
| `layout_force` | 160 节点 / 200 迭代 | 146 ms | **已知性能边界**：大图需 Barnes-Hut 近似或降低迭代数 |
| `mermaid_parse` | 200 / 400 / 800 / 1600 节点，四方言 | — | 方言解析扩展性基线；`--quick` 进 ctest |

## 五、规格差距总览（由代码清点得出，非估计）

- 按 `结构.md` 的具名文件表核对：`include/` 现有 **314 头**，`src/` **271 个 `.cpp`**。
  规格具名头文件约 336–338 个，故**尚缺约 26–28 个**（两种数法差 2，故写作区间）；
  `dsl/ widget/ plugin/ app/` 四个目录**均已落地**（此前为零代码），只剩 `termchart_gui/`
  属于 GUI 平行栈、刻意未开工。
- `examples/`：规格 60 个，实存 **57 个源**（规格编号示例 01–54 全部齐备；
  `03_function_plot`、`04_heatmap`、`05_showcase`、`06_dashboard`、`07_fractal`、
  `08_fractal_levels`、`40_advanced_master` 等为规格外补充，部分来自并行会话）。
- `bench/` **3 条基线**（`bench_sugiyama` / `bench_force` / `bench_mermaid_parse`），
  零外部依赖、`--quick` 进 ctest 冒烟。
- `tests/fuzz/` **8 个目标**，`--self-test` 已进 ctest（`ctest --preset fuzz`），
  跑过 6×20000 组随机输入无崩溃；语料 24 条共享 + PNG 专项 6 条。
- `tests/fixtures/`：夹具即生成物。**7 份规格夹具已全部补齐**——
  `gen_dsl_samples.py` 产 4 份（mermaid / dot / plantuml / csv），
  `gen_fixtures.py` 产 3 份（`sample_data.hpp` / `sample_graph.hpp` / `sample_grapheme.txt`），
  两者都进 `check_generated`；消费方 `tests/unit/test_fixtures.cpp` 守夹具自身不变量。
- `TODO` / `FIXME` 在 `include/`、`src/`、`examples/`、`tests/` 内为 **0**；占位类关键词命中 24 处，
  其中**真实缺口 2 处**（`scene_renderer` 的 `ImageOp`（Scene 里没有像素源）、
  `layout/internal.hpp` 的占位线格字符），其余为文档化阶段占位或正常渲染逻辑。
  `image/{png,png_encode,decoder}` 与 `protocol/iterm2_inline` 曾在此清单里，现已实现：
  PNG 编码器用 Python 参考 zlib 校验过输出，解码器用 `tools/gen_png_samples.py` 造的
  12 个样本（滤波 1–4、动态 Huffman、调色板 4/8 位、16 位灰度、坏 CRC、Adam7）交叉验证。
- 单头发布**已建立**：`single_include/termchart.hpp` 由 `tools/gen_single_header.py` 生成
  （进 `check_generated`），示例 `examples/42_single_header` 在 ctest 里冒烟。
- **平台现实（不是代码问题）**：本机 MinGW-w64 GCC 15.2 **不带 `libasan` / `libubsan` / `libtsan`**，
  故 ASan/UBSan/TSan **无法在 Windows 上跑**；覆盖率可跑，但 gcov 运行时写不进含非 ASCII
  字符的路径，需 `GCOV_PREFIX` 指向 ASCII 目录（实测 382 份 `.gcda` 正常产出）。
  ASan / TSan / coverage 三项已进 `.github/workflows/ci.yml` 的 ubuntu 作业。

---

## 六、GUI 接入基础（层号 0–4）

`结构.md` 第二十八节定死了 GUI 栈的复用边界：GUI 复用 **`scene` 及以下**
（`layout / widget / interaction / app` 在 GUI 下**重写**，这是设计选择、不是待解耦项）。
按 `cmake/TermchartModules.cmake` 的权威层号，这条边界就是 **L0–L4**：
`core(0) util(1) color(2) scale(3) scene(4)` —— 注意 `结构.md` 正文里写的
"scene（L2）"是**章节号**而不是层号（该文档每个模块的小节标题用的是章节序号），
真正的层号只在 `TermchartModules.cmake` 里定义，`check_layers.py` 按它校验。
本仓在这一边界内的准备情况：

| GUI 需要的能力 | 本仓提供 | 位置 |
| --- | --- | --- |
| 矢量绘制 IR（GUI 侧的 `scene_renderer` 消费它） | `Scene` / 命令变体 / `SceneBuilder` / 访问者 | `scene/`（L4） |
| 知道"这份 Scene 多大"（决定画布与无效区） | `scene_bounds()`（线段取端点、圆弧取圆心±半径方框、文本按显示宽度、不计描边宽度） | `scene/bounds.hpp` |
| `core::Style` → GUI 样式（前景/背景真彩、背景是否透明） | `resolve_style()` / `resolve_color()`（**先按 ColorDepth 量化再取色**，保证 GUI 与终端观色一致；`None` 用默认色而非黑） | `color/resolve.hpp`（L2） |
| 位图显示、缩放、拼版、叠加 | `Bitmap`：`fill_rect` / `blit_over`（source-over）/ `scaled_to`（最近邻）/ `with_alpha` / `without_alpha` | `image/bitmap.hpp`（L5，header-only） |
| 图片与连续量的互转（GUI 的图片控件 ↔ 终端的盲文/密度预览） | `luminance_field()`（Rec.601，alpha 乘进去）/ `field_to_bitmap()`（阈值双色，可选 alpha 取自场值） | `image/convert.hpp`（L5，header-only） |
| 平滑/网格化渲染（同一份标量场，换光栅器就是换分辨率） | `Rasterizer` 接口 + 五种实现 + `RasterRegistry`（可注册自定义光栅器） | `raster/`（L6） |
| 坐标类型、几何、计时、调度、取消 | `Point`/`Rect`/`Rgb`/`Style`、`util/geom`、`util/timer`、`util/scheduler`、`util/task` | `core/`、`util/` |

**已落地（2026-10-14）**：渲染核心改在 `include/termchart/gui/`（层 22）先期开工——`gui/font` 与 `gui/scene_renderer`（复用 `scene::StatefulVisitor` 时序），`examples/55_gui_scene_png` 全链路出图；上表共享面（scene 时序 / resolve / bounds / Bitmap / convert / raster）已接线。

**仍待开工**（GUI 侧剩余工作量）：`include/termchart_gui/` 与 `src_gui/` 规划共 25 个文件，目前涉及
（伞形头、`render/scene_renderer` `render/style_map`、`widget_tree/*`、`native_loop/*`、
`layout/*`（GUI 自己的约束/弹性/网格）、`backend_tk/*`）。规格把它们标为**平行项目**，
不阻塞库本体。

> 提醒：GUI 侧不要把终端路径的 `cell`/`SGR` 概念带进去。终端字符格是"低分辨率 +
> 颜色对"的表示法，GUI 的原子是**像素矩形 + 真彩色**；两者共用的应该是 `Scene`
> 与 `Style` 这类语义类型，而不是 `CellBuffer`。需要位图时用 `color::resolve` +
> `image::convert` 这条链，而不是去解析 ANSI 序列。
