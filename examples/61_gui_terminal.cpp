// 61_gui_terminal.cpp —— termchart_gui 平行栈经 TuiBackend 真彩出到终端
//
// 与 57 同源的一棵控件树（Label + Button + Slider），但改用 TuiBackend：把 GUI 画好的
// image::Bitmap 用 Unicode 上半块（▀）逐格刷到终端——每格上/下两像素映射为前景/背景
// 真彩色，垂直分辨率翻倍。这是零依赖、真正"看得见"的参考后端（真窗口 Qt/Tk/Win32 属
// 可选插件，不进核心）。
//
// 跑法：58_gui_terminal [out.txt]；不带参数只刷屏（Windows Terminal / VS Code 终端观感
// 最佳）。返回 0/1。
#include <cstdio>
#include <fstream>
#include <string>

#include "termchart_gui/backend_tk/tui_backend.hpp"
#include "termchart_gui/gui.hpp"
#include "termchart_gui/widget_tree/button.hpp"
#include "termchart_gui/widget_tree/label.hpp"
#include "termchart_gui/widget_tree/slider.hpp"

namespace {

    using namespace termchart_gui;

} // namespace

int main(int argc, char* const* argv) {
    // 120×48 像素画布（halfblock → 120 列 × 24 文本行）。
    TuiBackend backend(120, 48, termchart::ColorDepth::TrueColor);

    Container root("root");
    root.set_bounds(Rect{0, 0, 120, 48});
    root.emplace_child<Label>("title", "termchart_gui · TUI backend", Rgb{240, 240, 245},
                              Rgb{28, 32, 40});

    auto& btn = root.emplace_child<Button>("btn", "Click me");
    int clicks = 0;
    btn.set_on_click([&] { ++clicks; });

    auto& slider = root.emplace_child<Slider>("slider");
    double value = 0.0;
    slider.set_value(0.35);
    slider.set_on_change([&](double v) { value = v; });

    // 挂载并整帧绘制（clear + relayout + 各 Widget::paint + present 上屏）。
    Mount mount(root, backend);
    mount.paint_all();

    // 演示事件路由：在按钮中心点一下，触发 on_click。
    const Event click = tk_event_map::mouse_press(60.0, 18.0);
    const bool hit = mount.dispatch_event(click);
    if (hit) {
        mount.paint_all(); // 按钮进入按下态 → 重绘
    }

    if (argc > 1) {
        // 把同一帧的 halfblock 文本落盘，便于在无真彩终端里核对内容。
        const Bitmap bmp = backend.canvas().snapshot();
        std::ofstream out(argv[1], std::ios::binary);
        if (!out) {
            std::fprintf(stderr, "cannot open %s\n", argv[1]);
            return 1;
        }
        out << bitmap_to_halfblock(bmp, termchart::ColorDepth::TrueColor);
        if (!out) {
            std::fprintf(stderr, "failed to write %s\n", argv[1]);
            return 1;
        }
        std::printf("wrote %s (clicks=%d, slider=%.2f)\n", argv[1], clicks, value);
    } else {
        backend.present();
        std::fprintf(stderr, "frame presented (clicks=%d, slider=%.2f)\n", clicks, value);
    }
    return 0;
}
