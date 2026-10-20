// tk_demo.cpp —— 原生 Tk 后端可运行演示（真实窗口 + Tcl/Tk 事件驱动）
//
// 与 win32_demo / qt_demo 同一套控件接线（widget_tree/Mount），事件源换成
// Tcl 消息泵：鼠标/键盘经 `bind` → termchart_tk_event 命令翻成 Event 入队，
// TkBackend::poll_event → Mount::dispatch_event 路由到控件；present 用
// Tk_PhotoPutBlock 把位图贴进 photo image。演示层零平台代码。
//
// 跑法：tk_demo            → 开窗口交互（ESC 或关闭按钮退出）
//       tk_demo --smoke    → 无交互冒烟：泵数帧事件、重绘后自动关闭（供 CI/验证）
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
#include "termchart_gui_tk/tk_backend.hpp"

int main(int argc, char** argv) {
    using namespace termchart_gui;
    (void)argc;
    const bool smoke = (argc > 1 && std::string(argv[1]) == "--smoke");
    const auto frame_pause = std::chrono::milliseconds(smoke ? 20 : 8);

    termchart_gui_tk::TkBackend backend(480, 320, "termchart_gui - Tk");
    if (!backend.valid()) {
        std::fprintf(stderr, "TkBackend: Tk_Init 失败（需要显示环境与 Tcl/Tk 库）\n");
        return 2;
    }

    Container root("root");
    root.set_bounds(Rect{0, 0, 480, 320});
    root.emplace_child<Label>("title", "Native Tk GUI", Rgb{240, 240, 245}, Rgb{28, 32, 40});
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
    mount.paint_all();  // paint_all 内含 present()

    // 尺寸变化同步重渲染（<Configure> → Tcl 命令 → Impl::push 就地回调）。
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
