// qt_demo.cpp —— 原生 Qt 后端可运行演示（真实窗口 + Qt 事件驱动）
//
// 与 win32_demo 同一套控件接线（widget_tree/Mount），事件源换成 Qt 消息泵：
// 鼠标点击/拖拽、滚轮（Wheel 事件）、键盘经 QtBackend::poll_event →
// Mount::dispatch_event 路由到控件；状态变化后 paint_all 重绘、present 触发
// QWidget 重绘。演示层零平台代码（对所有后端同一份）。
//
// 跑法：qt_demo            → 开窗口交互（ESC 或关闭按钮退出）
//       qt_demo --smoke    → 无交互冒烟：泵数帧事件、重绘后自动关闭（供 CI/验证）
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/gui.hpp"
#include "termchart_gui/widget_tree/button.hpp"
#include "termchart_gui/widget_tree/checkbox.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/label.hpp"
#include "termchart_gui/widget_tree/mount.hpp"
#include "termchart_gui/widget_tree/progress_bar.hpp"
#include "termchart_gui/widget_tree/slider.hpp"
#include "termchart_gui_qt/qt_backend.hpp"

int main(int argc, char** argv) {
    using namespace termchart_gui;
    (void)argc;
    const bool smoke = (argc > 1 && std::string(argv[1]) == "--smoke");
    const auto frame_pause = std::chrono::milliseconds(smoke ? 20 : 8);

    termchart_gui_qt::QtBackend backend(480, 320, "termchart_gui - Qt");
    if (!backend.valid()) {
        std::fprintf(stderr, "QtBackend: 无法初始化（需要图形环境）\n");
        return 2;
    }

    Container root("root");
    root.set_bounds(Rect{0, 0, 480, 320});
    root.emplace_child<Label>("title", "Native Qt GUI", Rgb{240, 240, 245}, Rgb{28, 32, 40});
    auto& slider = root.emplace_child<Slider>("slider");
    auto& bar = root.emplace_child<ProgressBar>("bar");
    auto& val = root.emplace_child<Label>("val", "0%");
    auto& btn = root.emplace_child<Button>("btn", "Count: 0");
    auto& chk = root.emplace_child<Checkbox>("chk", "Show progress");

    int counter = 0;
    slider.set_on_change([&](double v) {
        bar.set_value(v);
        val.set_text(std::to_string(static_cast<int>(v * 100.0)) + "%");
    });
    btn.set_on_click([&] {
        ++counter;
        btn.set_label("Count: " + std::to_string(counter));
    });
    chk.set_on_toggle([&](bool on) { bar.set_visible(on); });

    Mount mount(root, backend);
    mount.paint_all();  // paint_all 内含 present()（清屏→重排→绘制→上屏）

    // 尺寸变化同步重渲染（与 Win32 同款钩子语义：Qt 的 resizeEvent 里就地回调）。
    backend.set_render_hook([&](std::uint32_t w, std::uint32_t h) {
        root.set_bounds(Rect{0, 0, static_cast<double>(w), static_cast<double>(h)});
        mount.paint_all();
    });

    auto drain = [&](bool& running) -> bool {
        bool dirty = false;
        Event e;
        while (backend.poll_event(e)) {
            if (e.type == EventType::Quit) {
                running = false;
                return dirty;
            }
            (void)mount.dispatch_event(e);
            dirty = true;
        }
        return dirty;
    };

    if (smoke) {
        bool running = true;
        for (int i = 0; i < 5 && running; ++i) {
            backend.process_events();
            const bool dirty = drain(running);
            (void)dirty;
            mount.paint_all();
            std::this_thread::sleep_for(frame_pause);
        }
        backend.request_close();
        std::fprintf(stderr, "smoke: frame rendered, window closed cleanly\n");
        return 0;
    }

    bool running = true;
    while (running) {
        backend.process_events();
        const bool dirty = drain(running);
        if (dirty) {
            mount.paint_all();
        }
        std::this_thread::sleep_for(frame_pause);
    }
    return 0;
}
