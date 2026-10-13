# AGENTS.md —— 架构摘要快照（专供 AI）

本文件是给 AI 助手的最小上下文。**遇到冲突时**：
进度以 `ROADMAP.md` 为准，规格以 `结构.md` 为准，细节陷阱以 `CODEBUDDY.md` 为准。

## 项目一句话

termchart：零强制依赖的 C++20 终端图表库。在终端里画真图（盲文 2×4 /
半块 1×2 子像素），同一份数据有终端实时 / 矢量导出 / 位图导出三条出口。

## 模块与层号（唯一真源：cmake/TermchartModules.cmake）

| 层 | 模块 | 一句话 |
| --- | --- | --- |
| L0 | core | CellBuffer / Cell / Style / Expected / arena |
| L1 | util | utf8 · grapheme · wcwidth · geom · timer |
| L2 | color | palette · quantize · colormap · resolve |
| L3 | scale | 线性 / 对数 / nice / 刻度 + 注册表 |
| L4 | scene | 矢量 IR（命令 + 访问者 + builder） |
| L5 | image | Bitmap · FloatField · PNG 编解码（自带 inflate） |
| L6 | raster | FloatField → 字符 + 注册表 |
| L7 | term | raw mode · caps · ANSI · OSC |
| L8 | document | Document / Block / Span |
| L9 | protocol | sixel · kitty · iTerm2 · 半块 · 探测 |
| L10 | canvas | 画布 · 图元 · 变换 · 裁剪 · Scene→Canvas |
| L11 | layout | 轴 / 网格 / 图例 / 标题 |
| L12 | chart | 折线 / 柱状 / 散点 / 迷你线 + 注册表 |
| L13 | graph | 节点边模型 · 布局 · 路由 · 图种 |
| L14 | render | 全量 / 增量渲染 · diff · SGR（唯一生成点） |
| L15 | interaction | 事件 / 分发 / 命中测试 |
| L16 | backend | 三物种后端 |
| L17 | dsl | lexer / parser / mermaid |
| L18–L20 | widget / plugin / app | GUI 阶段（规格在册） |
| L21 | api | print · quick（一行式） |

规则：只能 include 层号 ≤ 自己的模块；禁止边 `backend>chart`、
`chart>render`、`render>interaction`；`detail/`（层号 -1）任何层可 include。

## 护栏命令（改代码前先会跑）

```bash
cmake --preset ninja && cmake --build --preset ninja -j
ctest --preset ninja                                   # 全部测试与示例冒烟
cmake --build --preset ninja --target check_all        # 分层+自洽+生成物+每头独立编译
cmake --build --preset ninja --target generate_all     # 重跑生成器（改了生成器后必跑）
cmake --build --preset ninja --target check_headers    # 每头独立编译
python tools/gen_golden.py                             # 重生成 golden 快照
```

## 硬性约定（违者护栏会拦）

1. **生成物禁手改**：`include/termchart/detail/*.hpp`、`结构.md` 附一/附四、
   `docs/reference/module_matrix.md`、`single_include/`、`tests/golden/`、
   `cmake/header_only.txt` 的联动生成物——只能重跑生成器。
2. **新增公开头 = 四件套**：CMake 登记 + `src/` 同名 cpp + 单测 + 每头独立编译；
   纯头要进 `cmake/header_only.txt`（随后必须重跑 `generate_all`）。
3. **SGR 只从 `render/sgr.hpp`** 出；平台 `#ifdef` 只进 `src/term/*_posix.cpp` /
   `_win32.cpp`，公开头不写。
4. **错误处理**：只报成败用 `Status`，带值用 `Result<T>`（`core/expected.hpp`）；
   不向调用方抛异常。
5. **宽字符**：写 buffer 的生产者必须自己填续接格 `kContinuationCp`。
6. GUI 复用边界是 L0–L2（scene 及以下）；不要把 cell/SGR/CellBuffer 带进 GUI。

## 进度口径（2026-10-13 尾段快照，复核以 ROADMAP「修订记录」表为准）

最新一行：`ctest` **73/73 全绿**、doctest **848 用例 / 191580 断言、0 失败**、
四护栏全绿（`check_header_only` **312 头 / 269 实现**）；**阶段 0–7 出口条件全部闭合**，
阶段 8（GUI 平行栈）未开工。文件普查：`include/` 310 头、`src/` 267 `.cpp`、
`examples/` 57 源、`tests/` 126 源、golden 9 份。

> 注意：`ROADMAP.md`「一、项目现状」小节仍是 2026-10-11 口径
> （称 L17–L20 为零代码空洞），**已过时**；读进度只看「修订记录」表。

能力矩阵：`docs/reference/capability_matrix.md`；缺口总览：`docs/reference/backlog.md`；
质量体检：`docs/reference/quality_todo.md`。
