# Contributing to termchart

谢谢参与。本项目有一套**机器强制**的护栏，贡献流程围绕它展开；
先读 [`ROADMAP.md`](ROADMAP.md)（唯一进度口径）与 [`结构.md`](结构.md)（规格）。

## 开发环境

- C++20，CMake ≥ 3.20，Ninja（推荐，跨平台预设 `ninja`）；
- Windows 上 GNU make 在含非 ASCII 路径下不可靠，**必须用 Ninja 预设**；
- 零强制依赖：不要引入任何第三方库（vendored doctest 除外）；
  可选依赖只走 `find_package`（`cmake/FindTermchartDeps.cmake`）。

## 构建与验证

```bash
cmake --preset ninja
cmake --build --preset ninja -j
ctest --preset ninja

# 护栏三件套（分层 + 头文件自洽 + 生成物一致 + 每头独立编译）
cmake --build --preset ninja --target check_all
```

## 分支模型

- `main`：始终可构建、`ctest` 全绿；只经 PR 合入；
- 功能分支：`feat/<主题>`、`fix/<主题>`、`docs/<主题>`，从 `main` 切出，
  一个分支一件事，保持垂直切片（对应 `ROADMAP.md` 的 P2 原则）。

## 提交规范

- 提交信息：一行主题 + 空行 + 必要说明；主题用祈使句，中文或英文均可；
- **牵涉进度计划的改动**（勾选阶段条目、新增计划）：PR 中同步在
  `ROADMAP.md` 顶部「修订记录」加一行，不直接改勾选状态以外的内容；
- 不提交生成物变更与源码变更混在一起的大杂烩提交（生成物走独立提交，
  并在信息里注明生成器与命令）。

## 生成物禁手改

以下全部由脚本产出，手改会被 `check_generated` 拦下：
`include/termchart/detail/*.hpp`、`结构.md` 附一/附四、
`docs/reference/module_matrix.md`、`single_include/`、`tests/golden/`。
需要更新时重跑生成器：

```bash
cmake --build --preset ninja --target generate_all
python tools/gen_golden.py        # 或 --target generate_golden
```

## 新增文件的"四件套"（P3 原则）

新增一个公开头文件 = 同时完成：

1. 登记进 CMake（纯头文件还要进 `cmake/header_only.txt`，并重跑 `generate_all`）；
2. `src/` 同路径同名 `.cpp`（含基类虚表锚点）；
3. 单元测试（doctest，按模块放 `tests/unit/<模块>/`）；
4. 通过 `check_headers`（每头独立编译）。

另须遵守分层规则（`cmake/TermchartModules.cmake`，`tools/check_layers.py` 强制），
尤其三条禁止边：`backend > chart`、`chart > render`、`render > interaction`；
SGR 序列只从 `render/sgr.hpp` 生成；平台差异只进 `src/term/*_posix.cpp` / `_win32.cpp`，
公开头不写平台 `#ifdef`。

## PR 清单

提交 PR 前逐项确认：

- [ ] `ctest --preset ninja` 全绿（引用具体数字，如 `ctest 50/50`，不写"应该没问题"）；
- [ ] `check_all` 通过（分层 / 自洽 / 生成物 / 每头独立编译）；
- [ ] 全量重编 0 warning；
- [ ] 新增头文件完成四件套；改过 `#include` 结构则 `check_layers` 无新豁免；
- [ ] 动过 golden 外观则已用 `gen_golden.py` 重新生成并附对比说明；
- [ ] 涉及进度的勾选已同步 `ROADMAP.md` 修订记录；
- [ ] `CHANGELOG.md` 增加了对应条目；
- [ ] 汇报口径诚实：未跑的检查明确说"未跑"及原因。

## 测试约定

- 框架只有 vendored doctest；golden 只比解析回的属性，不比字节；
- doctest 表达式分解不接受 `CHECK(a || b)`，要写 `CHECK((a || b))`；
- 并行/多会话开发时注意：全量构建可能因他人半截文件瞬时变红，
  复核时先重链再排查（见 `CODEBUDDY.md` 第 25 条）。
