# 编译选项

## 预设（CMakePresets）

| 预设 | 用途 |
| --- | --- |
| ninja / ninja-release | 首选（跨平台 Ninja；Windows 上比 *-windows 预设可靠） |
| debug-windows | MinGW Makefiles（路径含非 ASCII 时不可用——GNU make 按 ANSI 破坏参数） |
| asan / tsan / coverage | 仅非 Windows 预设 |
| fuzz | fuzz 套件（13 项，含 6 目标自检） |

构建目录 = `build/<presetName>`。

## 模块开关

`cmake/TermchartOptions.cmake`：

- `TERMCHART_MINIMAL_BUILD=ON`：只保留 core/util/color/term/render/api；
- 可选依赖 libpng / zlib / utf8proc：默认 OFF，缺失只降级不失败。

## 定义

- `TERMCHART_STATIC`：静态构建时的 PUBLIC 定义（符号可见性，
  见 export.hpp 与 design/abi_policy.md）；
- `TERMCHART_UPDATE_GOLDEN=1`（环境变量）：golden 用例自写快照
  （由 golden_runner.cpp 处理，禁手改快照文件）。

## 常用构建目标

| 目标 | 内容 |
| --- | --- |
| check_all | 分层校验 + 每头独立编译 + 生成物一致（三件套总入口） |
| check_layers | tools/check_layers.py（禁止边/收窄边/层号） |
| check_headers | 每个公开头单独 TU `-fsyntax-only` |
| check_generated | gen_*.py --check（unicode/grapheme/braille/module_graph/golden） |
| generate_all | 重跑全部生成器 |
| generate_golden | 重录 golden（等价 python tools/gen_golden.py） |

## include 路径

- 使用方：加 `include/`；
- 仓库内 `src/term/platform_internal.hpp` 以 `src/...` 相对路径被 include
  （顶层 CMakeLists 把仓库根加进 PRIVATE include 目录）——这是私有约定，
  使用方代码不应依赖 `src/` 下的任何头。

## 环境备注（本仓库开发机）

- cmake 不在 PATH：`%APPDATA%\Roaming\Python\Python314\site-packages\cmake\data\bin\`；
- ninja 在 `%APPDATA%\Roaming\Python\Python313\Scripts\ninja.exe`；
- 编译器：`C:\Program Files\mingw64\bin\c++.exe`（GCC 15.2）。
