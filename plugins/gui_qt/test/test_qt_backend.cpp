// plugins/gui_qt/test/test_qt_backend.cpp
//
// Qt 后端离屏回归测试（无显示器）：验证两处历史崩溃已修复——
//   1) 构造/显示阶段 QString 析构的堆损坏：UCRT 构建的 exe 与 msvcrt 版 Qt 混用
//      C 运行时（跨 DLL 的 malloc/free 落不同堆 → RtlFreeHeap 无效地址）。根因是
//      Qt 必须与构建 exe 的编译器同 CRT；本仓用 MSYS2 的 UCRT Qt 6.x，与 GCC 15.2
//      的 UCRT 对齐。
//   2) 拖拽/resize 触发重排重绘时的越界写堆损坏：画布缓冲未随窗口尺寸重分配，控件树
//      按新尺寸排版却画进固定大小缓冲。
//
// 测试在 offscreen 平台下跑「启动 + 程序化 resize → 重排重绘」，干净退出即视为通过。
// CMake 已为 ctest 注入环境：把 Qt 的 bin 提到 PATH 最前（使其自带 libstdc++ 优先于
// 其它 MinGW，规避 GCC 版本错配导致的 STATUS_ENTRYPOINT_NOT_FOUND），并设 offscreen
// 平台与插件路径，便于无显示器 CI 复现。
//
// 退出码：0=通过；1=逻辑失败；2=环境缺失（无 Qt/无显示器），视为 SKIP 而非失败。

#include <cstdio>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/gui.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/label.hpp"
#include "termchart_gui/widget_tree/mount.hpp"
#include "termchart_gui/native_loop/loop.hpp"
#include "termchart_gui_qt/qt_backend.hpp"

int main() {
    using namespace termchart_gui;

    termchart_gui_qt::QtBackend backend(480, 320, "termchart_gui_qt regression");
    if (!backend.valid()) {
        std::fprintf(stderr,
                     "[qt_backend_test] QtBackend invalid (Qt missing or no display) — SKIP\n");
        return 2;
    }

    Container root("root");
    root.set_bounds(Rect{0.0, 0.0, 480.0, 320.0});
    root.emplace_child<Label>("title", "Native Qt GUI", Rgb{240, 240, 245},
                              Rgb{28, 32, 40});

    auto window = backend.create_window(480, 320, "termchart_gui_qt regression");
    Mount mount(root, backend);
    mount.paint_all();

    // 尺寸变化时：按新尺寸重排版 + 重绘（Qt 后端已在内部先把画布缓冲 resize，
    // 这里确认控件树也不越界）。
    backend.set_render_hook([&](std::uint32_t w, std::uint32_t h) {
        root.set_bounds(Rect{0.0, 0.0, static_cast<double>(w), static_cast<double>(h)});
        mount.paint_all();
    });

    int frame = 0;
    backend.set_frame_callback([&]() {
        ++frame;
        if (frame == 30) {
            window->resize(1000, 700);  // 触发 resizeEvent → render_hook → 重排重绘
        }
        if (frame == 150) {
            backend.request_close();
        }
    });

    HeadlessEventLoop loop;
    backend.run_loop(loop);

    if (frame < 30) {
        std::fprintf(stderr, "[qt_backend_test] loop exited early at frame %d\n", frame);
        return 1;
    }
    std::printf(
        "[qt_backend_test] OK: completed %d frames incl. resize to 1000x700, clean exit\n",
        frame);
    return 0;
}
