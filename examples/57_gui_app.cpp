// 57_gui_app.cpp —— termchart_gui 平行栈端到端演示（无头）
//
// 这条链路与终端栈完全平行：不经 canvas/term/cell，控件树由 widget_tree 自管，
// 绘制走 TkCanvas（无头实现是 image::Bitmap），最外层用 HeadlessBackend 把"窗口+
// 事件源+上屏"都桩成无窗口系统的形态。本示例验证整栈能跑通：
//   控件树 → Container::relayout（默认纵向 flex）→ Widget::paint → 快照 → PNG。
//
// 跑法：57_gui_app [out.png]；不带参数只校验编码、不落盘（ctest 用），返回 0/1。
#include <cstdio>
#include <fstream>
#include <string>

#include "termchart/image/png_encode.hpp"
#include "termchart_gui/gui.hpp"

namespace {

using namespace termchart_gui;
using termchart::image::encode_png;

// 一个会把自己画成纯色块的控件：演示 Widget 的 paint 钩子（即时模式，走 TkCanvas，
// 不碰终端的 cell/SGR）。
class Box : public Widget {
public:
    Box(std::string id, Rgb fill) : Widget(std::move(id)), fill_(fill) {}

    void paint(TkCanvas& canvas) const override {
        canvas.fill_rect(global_bounds(), fill_);  // 用全局坐标画自己
    }

private:
    Rgb fill_;
};

}  // namespace

int main(int argc, char** argv) {
    // 1) 建控件树：根容器 + 三个纯色块（纵向 flex 自动排成三条横带）。
    Container root("root");
    root.set_bounds(Rect{0, 0, 240, 160});

    root.emplace_child<Box>("red", Rgb{200, 60, 60});
    root.emplace_child<Box>("green", Rgb{60, 180, 90});
    root.emplace_child<Box>("blue", Rgb{70, 120, 210});

    // 再嵌一个子容器，演示递归 relayout：子容器里放两个窄色块。
    auto& group = root.emplace_child<Container>("group");
    (void)group;  // 默认纵向 flex
    group.emplace_child<Box>("g1", Rgb{230, 200, 70});
    group.emplace_child<Box>("g2", Rgb{120, 90, 200});

    // 2) 无头后端：240×160 画布 = 内部 image::Bitmap。
    HeadlessBackend backend(240, 160);

    // 3) 挂载并整帧绘制：clear + relayout + 各 Widget::paint + present。
    Mount mount(root, backend);
    mount.paint_all();

    // 4) 取快照 → 编码 PNG。事件路由示例（证明 native_loop 也接上了）：
    //    在根内点一下，看是否命中某个控件。
    const Event click = tk_event_map::mouse_press(120.0, 80.0);
    const bool hit = mount.dispatch_event(click);
    (void)hit;  // 演示用：真实应用里这里会触发 Widget::on_event

    const Bitmap bmp = backend.canvas().snapshot();
    const std::string png = encode_png(bmp);
    if (png.empty()) {
        std::fprintf(stderr, "encode_png produced no bytes\n");
        return 1;
    }

    if (argc > 1) {
        std::ofstream out(argv[1], std::ios::binary);
        if (!out) {
            std::fprintf(stderr, "cannot open %s\n", argv[1]);
            return 1;
        }
        out.write(png.data(), static_cast<std::streamsize>(png.size()));
        if (!out) {
            std::fprintf(stderr, "failed to write %s\n", argv[1]);
            return 1;
        }
        std::printf("wrote %s (%zu bytes)\n", argv[1], png.size());
    } else {
        std::printf("encoded %zu bytes (no output path given)\n", png.size());
    }
    return 0;
}
