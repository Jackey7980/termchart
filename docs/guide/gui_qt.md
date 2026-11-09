# 构建原生 Qt GUI 后端

`termchart` 的 Qt 后端是一个**可选插件**（`plugins/gui_qt/`），默认不编译。开启后会在
`examples/qt_demo.exe` 里演示一个原生 Qt 窗口。本页讲清楚怎么把它编出来、跑起来，以及
**最关键的一个坑：Qt 的 C 运行时（CRT）必须与构建核心库的编译器一致**。

## 1. 为什么必须用 UCRT 版 Qt（否则崩溃）

核心库与所有 demo 用本机的 **GCC 15.2（纯 UCRT）** 构建——编出来的 exe 导入
`api-ms-win-crt-*`。而 Qt.io 官方提供的 `mingw_64` 预编译包（`D:\tools\Qt\...`）是用更老的
MinGW（**msvcrt**）编的，其 `Qt6Core.dll` 导入 `msvcrt.dll`。

同一进程里混用两个 C 运行时 → 跨 DLL 的 `malloc/free` 落进**不同堆** → `RtlFreeHeap` 拿到
无效地址 → **堆损坏崩溃**，回溯恒指向 `QString::~QString → ucrtbase!_free_base`。拖动窗口 /
resize 只是更早触发（更多 Qt 分配），并非逻辑 bug。gdb 实测即此根因。

> 注：本机 GCC 15.2 是 UCRT-only 工具链，`-mcrtdll=msvcrt` 旗标被彻底忽略，无法通过换旗标
> 让 exe 切到 msvcrt。所以**唯一干净的解法是让 Qt 也用 UCRT**。

因此请使用 **MSYS2 的 UCRT Qt**：`mingw-w64-ucrt-x86_64-qt6-base`。它与 GCC 15.2 同为 UCRT，
跨堆问题消失。

## 2. 安装 UCRT Qt（可复现）

Qt 二进制**不进版本库**（LGPL 动态链接、体积大，且 vendoring 违反「可选扩展由使用者自备」
的既定取向）。通过 MSYS2 按需获取即可，版本由 MSYS2 包管理器钉死，可复现：

```powershell
# 一键安装（含国内镜像加速 + 路径提示）：
powershell -ExecutionPolicy Bypass -File tools/setup_qt_ucrt.ps1
```

脚本做的事：把国内镜像（阿里云/中科大）提到 `mirrorlist.mingw` 最前（规避 MSYS2 主镜像对
新包的 403/超时），再 `pacman -S mingw-w64-ucrt-x86_64-qt6-base`。安装位置为
`C:\msys64\ucrt64`。

也可手工执行：

```bash
# 在 MSYS2 的 UCRT64 环境（或 msys2_shell / bash -lc）里：
pacman -Sy --noconfirm
pacman -S --noconfirm mingw-w64-ucrt-x86_64-qt6-base
```

## 3. 构建

```powershell
$env:QTDIR = 'C:\msys64\ucrt64'          # CMake 通过 $env{QTDIR} 找 Qt（预设不落绝对路径）
cmake --preset qt                        # 开启 TERMCHART_BUILD_GUI_QT，CMAKE_PREFIX_PATH=$env{QTDIR}
cmake --build build/qt --target qt_demo
```

`qt_demo` 会链接 `Qt6Core/Gui/Widgets`（动态）。构建目录为 `build/qt`。

> **切换/重装 Qt 后务必清掉旧缓存**：CMake 会把 `Qt6_DIR` 钉进 `CMakeCache.txt`。
> 若你曾用 msvcrt 版 Qt（如 `D:\tools\Qt\6.8.3`）配过 `build/qt`，再切到 UCRT Qt 时
> `find_package` 会沿用旧缓存 → 链接/加载错版 Qt，测试在 ctest 下仍堆损坏崩溃。
> 重配前先清缓存：
> ```powershell
> cmake -UQt6_DIR -UQt6Core_DIR -UQt6Widgets_DIR -UQt6Gui_DIR `
>      -UQt6CoreTools_DIR -UQt6WidgetsTools_DIR -UQt6GuiTools_DIR --preset qt
> ```

## 4. 运行（**PATH 顺序是关键**）

运行 Qt 程序时，**必须把 `C:\msys64\ucrt64\bin` 放到其它 MinGW 之前**：

```powershell
# 错误：若 mingw64\bin（GCC 15.2 的 libstdc++-6.dll）在前，Qt6Core 会加载 15.2 的
#       libstdc++ 而缺 MSYS2 Qt（GCC 16.2）的符号 → 启动即 STATUS_ENTRYPOINT_NOT_FOUND (0xC0000139)。
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
$env:QT_QPA_PLATFORM_PLUGIN_PATH = 'C:\msys64\ucrt64\share\qt6\plugins\platforms'

# 交互窗口：
build/qt/examples/qt_demo.exe
# 无显示器冒烟（打印 "frame rendered, window closed cleanly" 即成功）：
build/qt/examples/qt_demo.exe --smoke
```

> 这个 PATH 顺序坑同样适用于 CI 与本仓库的 ctest：插件的测试已自动把 Qt 的 bin 提到 PATH
> 最前，无需手动设置（见第 5 节）。

## 5. 回归测试

`ctest --preset qt` 会跑两个 Qt 相关用例（offscreen，无需显示器）：

- `qt_backend_test`：启动后端 + **程序化 `window->resize(1000,700)` 触发重排重绘**，干净退出即通过。
  这是「拖大窗口/resize 崩溃」的回归守护——同时覆盖 CRT 不匹配与画布缓冲未随窗尺寸重分配两处旧根因。
- `qt_demo_smoke`：跑 `qt_demo --smoke` 的离屏冒烟。

两个用例的测试环境由 `plugins/gui_qt/CMakeLists.txt` 注入（Qt bin 前置 + offscreen + 插件路径），
所以直接 `ctest --preset qt` 即可复现，不依赖你全局 PATH 怎么设。

## 6. 许可

Qt 为 LGPLv3 / GPLv3 / 商业三态授权，termchart 本体为宽松许可。插件只允许**动态链接** Qt
（MSYS2 包默认即是），`CMakeLists.txt` 已对静态 Qt fail-loud 拦截。分发时随附 LGPLv3 全文与
Qt 源码获取途径、且不阻止用户替换 Qt 库本身，即可保持本项目宽松许可。
