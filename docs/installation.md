# 安装与集成

## 构建（源码方式）

首选 Ninja 预设（跨平台、不依赖 GNU make）：

```sh
cmake --preset ninja
cmake --build --preset ninja
```

Windows 备用：`cmake --preset debug-windows`（MinGW Makefiles）。
**注意**：仓库路径含非 ASCII 字符时 GNU make 会按 ANSI 破坏参数，必须用 `ninja` 预设。

本机环境备注（若 cmake/ninja 不在 PATH）：cmake 可能位于
`%APPDATA%\Roaming\Python\Python314\site-packages\cmake\data\bin\`；
编译器示例为 `C:\Program Files\mingw64\bin\c++.exe`（GCC 15.2）。

## 最小构建

```sh
cmake -DTERMCHART_MINIMAL_BUILD=ON
```

只保留 core/util/color/term/render/api（开关在 `cmake/TermchartOptions.cmake`）。
可选依赖 libpng/zlib/utf8proc 默认 OFF，缺失只降级不失败。

## 集成方式

### 方式一：源码并入你的构建

把 `include/` 加入 include 路径、`src/**.cpp` 加入编译清单。目录约定：

- `include/termchart/**` 与 `src/**` 严格同路径同名，一个头一个 cpp；
- 少数纯头文件（如 `core/point.hpp`、`util/math.hpp`）在 `cmake/header_only.txt` 登记豁免。

### 方式二：单头使用

`single_include/` 是聚合产物（由脚本生成，禁手改），用法与代价见
[guide/single_header.md](../guide/single_header.md)。

### 方式三：CMake 子目录

```cmake
add_subdirectory(teamchart)
target_link_libraries(myapp PRIVATE termchart)
```

## 平台注意

- 终端上色：Windows 控制台需开启 VT（库的 `Terminal::enter()` 会调 `platform::enable_vt()`）；
  PowerShell 侧另需 `[Console]::OutputEncoding=[Text.Encoding]::UTF8`。
  最省事是用 Windows Terminal / VS Code 终端。
- 平台差异全部隔离在 `src/term/*_posix.cpp` / `*_win32.cpp`，公开头里没有 `#ifdef`。

## 测试与护栏

```sh
ctest --preset ninja                                  # 全量
cmake --build --preset ninja --target check_all       # 分层 + 每头独立编译 + 生成物一致
ctest --preset fuzz                                   # fuzz 套件（可选）
```
