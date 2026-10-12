# 分层与编译裁剪

## 分层即模块

模块 = `include/termchart/` 下的一级目录（层号表见 layering.md）。模块边界同时是
编译单元边界：

- `include/termchart/**` 与 `src/**` **严格同路径同名**，一个头一个 cpp
  （基类也要有 cpp 放虚表锚点）；
- 例外只有 `cmake/header_only.txt` 登记的纯头（`core/point|rect|attr|color_depth|rgb|span_utils`、
  `util/bits|math|random|assert`、`color/named_colors`、顶层 `detail/` 与若干子目录纯头）。

## 编译裁剪

`cmake/TermchartOptions.cmake` 提供模块开关；`-DTERMCHART_MINIMAL_BUILD=ON`
只保留 core/util/color/term/render/api。裁剪的意义：

- 嵌入式/CLI 工具只要终端上色闭环时，不编入 chart/graph/backend 等大模块；
- 分层校验保证低层模块永远不知道高层存在，裁掉高层不需要改低层代码。

## 每头独立编译

`tools/check_headers.py`（目标 `check_headers`）为 `include/termchart/**` 每个头
生成一个只含它的 TU 并 `-fsyntax-only`：任何头都必须自洽（自行 include 其依赖）。
新增/修改头文件后必跑——GUI 平行栈会把公开头单独 include 进自己的代码。

## 纯头登记的连环约束

新增 header-only 文件必须写进 `cmake/header_only.txt`；该清单同时是生成物
`结构.md` 附四的输入，所以**改完必须重跑 `generate_all`**：

- 只加清单不改生成物 → `check_generated` 失败；
- 手改生成物 → 更失败（生成物禁手改）。
注意判据：只豁免顶层 `termchart/detail/` 与无斜杠的顶层头；子目录纯头
（如 `interaction/detail/overlay_cells`）必须显式登记。

## 可扩展性契约

三组集成测试（`tests/integration/replace_*.cpp`）验证"新增 backend / chart / raster，
核心零改动"：注册表 + 工厂模式（chart/registry.hpp、raster/registry.hpp、
scale/registry.hpp），`instance()` 为 function-local static，构造后只读。
设计细节见 registry.md。

## 反例

- 头文件里 include 只在 cpp 里需要的依赖（破坏最小包含，也拖累每头独立编译）；
- 为绕过禁止边建"工具头"当跳板（check_layers 会沿 include 链追责）；
- 在公开头里加平台 `#ifdef`（平台差异走 `src/term/*_posix|win32.cpp`）。
