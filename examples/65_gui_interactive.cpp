// 65_gui_interactive.cpp —— termchart_gui 交互闭环演示（事件驱动 → 状态联动 → 重绘）
//
// 把平行栈的交互链路真正跑起来：Slider 拖动 → on_change 联动 ProgressBar 填充与
// Label 百分比；Button 点击 → 计数并自改标签；Checkbox 切换 → 控制 ProgressBar 可见性。
// 每一步 dispatch 一个合成鼠标事件（坐标取**布局后的真实 global_bounds**，稳健，不依赖
// 具体 flex 均分结果），再 paint_all 重绘。零依赖、无真实窗口，用 TuiBackend 出最终帧。
//
// 跑法：65_gui_interactive [out.txt]；不带参数刷终端，带参数把最终 halfblock 帧落盘。
#include <cstdio>
#include <fstream>
#include <string>

#include "termchart_gui/backend_tk/tui_backend.hpp"
#include "termchart_gui/gui.hpp"
#include "termchart_gui/widget_tree/button.hpp"
#include "termchart_gui/widget_tree/checkbox.hpp"
#include "termchart_gui/widget_tree/label.hpp"
#include "termchart_gui/widget_tree/progress_bar.hpp"
#include "termchart_gui/widget_tree/slider.hpp"

namespace {

using namespace termchart_gui;

// 在控件带内按比例取一个点（fx, fy ∈ [0,1]），用于合成鼠标事件。
Event click_in(const Widget& w, double fx, double fy) {
    const Rect g = w.global_bounds();
    return tk_event_map::mouse_press(g.x + g.w * fx, g.y + g.h * fy);
}

}  // namespace

int main(int argc, char** argv) {
    TuiBackend backend(160, 96, termchart::ColorDepth::TrueColor);

    Container root("root");
    root.set_bounds(Rect{0, 0, 160, 96});
    root.emplace_child<Label>("title", "Interactive GUI (event-driven)", Rgb{240, 240, 245},
                              Rgb{28, 32, 40});
    auto& slider = root.emplace_child<Slider>("slider");
    auto& bar = root.emplace_child<ProgressBar>("bar");
    auto& val = root.emplace_child<Label>("val", "0%");
    auto& btn = root.emplace_child<Button>("btn", "Count: 0");
    auto& chk = root.emplace_child<Checkbox>("chk", "Show progress");

    int counter = 0;
    double slider_value = 0.0;
    slider.set_on_change([&](double v) {
        slider_value = v;
        bar.set_value(v);
        val.set_text(std::to_string(static_cast<int>(v * 100.0)) + "%");
    });
    btn.set_on_click([&] {
        ++counter;
        btn.set_label("Count: " + std::to_string(counter));
    });
    chk.set_on_toggle([&](bool on) { bar.set_visible(on); });

    Mount mount(root, backend);
    mount.paint_all();  // 先布局一次，global_bounds 之后有效

    // 1) 拖动滑块到 30%：on_change → bar 填充 + val 文本联动更新。
    const bool s = mount.dispatch_event(click_in(slider, 0.30, 0.5));
    mount.paint_all();
    std::fprintf(stderr, "[slider] hit=%d value=%.2f label=%s\n", static_cast<int>(s),
                 slider_value, val.text().c_str());

    // 2) 点按钮：计数 +1，按钮自改标签。
    const bool b = mount.dispatch_event(click_in(btn, 0.5, 0.5));
    mount.paint_all();
    std::fprintf(stderr, "[button] hit=%d %s\n", static_cast<int>(b), btn.label().c_str());

    // 3) 点复选框：切换 ProgressBar 可见性。
    const bool c = mount.dispatch_event(click_in(chk, 0.5, 0.5));
    mount.paint_all();
    std::fprintf(stderr, "[checkbox] hit=%d checked=%d bar_visible=%d\n", static_cast<int>(c),
                 static_cast<int>(chk.checked()), static_cast<int>(bar.visible()));

    if (argc > 1) {
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
        std::printf("wrote %s (slider=%.2f, %s, chk=%d)\n", argv[1], slider_value,
                    btn.label().c_str(), static_cast<int>(chk.checked()));
    } else {
        backend.present();
        std::fprintf(stderr, "interactive frame presented (slider=%.2f, %s, chk=%d)\n",
                     slider_value, btn.label().c_str(), static_cast<int>(chk.checked()));
    }
    return 0;
}
