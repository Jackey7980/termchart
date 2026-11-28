// backend_tk/tui_backend.hpp —— 零依赖"真可见"后端：Bitmap → 终端 halfblock
//
// 为什么是它而不是 Qt/Tk/Win32：本库核心零强制依赖、header-only；接真实 GUI 工具
// 包意味着外部依赖 + 编译型平台代码，违背硬约束——按既定原则（GUI 走可选插件、
// 不破零依赖），真窗口应做成**可选插件**，不进核心。这里给出一个**零依赖、可测、
// 且真正在终端出像素**的参考后端：把 GUI 画好的 image::Bitmap 用 Unicode 上半块
// （U+2580 ▀）逐格刷到终端——每格上/下两像素映射为前景/背景真彩色，分辨率翻倍。
//
// 颜色经 color::resolve 按目标 ColorDepth 量化，保证"GUI 里看到的颜色"与终端实际
// 显示一致（GUI 不假装能显示真彩）。不引入 render/sgr：这里的 SGR 是自包含的真彩
// 序列（38;2 / 48;2），与"不把终端 SGR 机制带进 GUI 平行栈"的边界一致。
#ifndef TERMCHART_GUI_BACKEND_TK_TUI_BACKEND_HPP
#define TERMCHART_GUI_BACKEND_TK_TUI_BACKEND_HPP

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

#include "termchart/core/color_depth.hpp"
#include "termchart/core/style.hpp"  // Color 定义于此（Color::rgb / ansi16）
#include "termchart/color/resolve.hpp"
#include "termchart/media/image/bitmap.hpp"
#include "termchart_gui/backend_tk/backend.hpp"

namespace termchart_gui {

// 把位图编码成半块终端文本（真彩 SGR + ▀）。每 2 个像素行合成一行字符：上一行
// 像素为前景、下一行为背景。返回完整字符串（含每行末尾的 reset+换行），便于测试。
[[nodiscard]] inline std::string bitmap_to_halfblock(
    const Bitmap& bmp, termchart::ColorDepth depth = termchart::ColorDepth::TrueColor) {
    const termchart::color::ResolveOptions opts{depth};
    const std::uint32_t w = bmp.width();
    const std::uint32_t h = bmp.height();

    const auto quantize = [&](std::uint32_t x, std::uint32_t y) -> Rgb {
        const auto r = static_cast<std::uint8_t>(bmp.at(x, y, 0));
        const auto g = static_cast<std::uint8_t>(bmp.at(x, y, 1));
        const auto b = static_cast<std::uint8_t>(bmp.at(x, y, 2));
        return termchart::color::resolve_color(termchart::Color::rgb(r, g, b), opts);
    };
    const auto append_rgb = [](std::string& s, const Rgb& c) {
        s += std::to_string(c.r);
        s.push_back(';');
        s += std::to_string(c.g);
        s.push_back(';');
        s += std::to_string(c.b);
    };

    std::string out;
    for (std::uint32_t y = 0; y < h; y += 2) {
        for (std::uint32_t x = 0; x < w; ++x) {
            const Rgb top = quantize(x, y);
            const Rgb bottom = (y + 1 < h) ? quantize(x, y + 1) : top;
            out += "\x1b[38;2;";
            append_rgb(out, top);
            out += ";48;2;";
            append_rgb(out, bottom);
            out += 'm';
            out += "\xe2\x96\x80";  // ▀ 上半块：前景画上半、背景画下半
        }
        out += "\x1b[0m\n";
    }
    return out;
}

// 参考后端：把内部 BitmapCanvas 的帧刷到终端（halfblock）。无原生输入（poll 恒
// false），事件可经 inject_event 注入以脚本化演示。run_loop 跑完一帧后 present。
class TuiBackend final : public Backend {
public:
    TuiBackend(std::uint32_t w, std::uint32_t h,
               termchart::ColorDepth depth = termchart::ColorDepth::TrueColor)
        : canvas_(w, h), depth_(depth) {}

    [[nodiscard]] std::unique_ptr<TkWindow> create_window(std::uint32_t, std::uint32_t,
                                                          std::string) override {
        return nullptr;  // 无独立窗口：整栈即这块画布
    }
    [[nodiscard]] TkCanvas& canvas() override { return canvas_; }
    [[nodiscard]] bool poll_event(Event&) override { return false; }

    void present() override {
        const Bitmap bmp = canvas_.snapshot();
        const std::string frame = bitmap_to_halfblock(bmp, depth_);
        std::fputs("\x1b[H", stdout);  // 光标归位：连续帧覆盖而非追加
        std::fwrite(frame.data(), 1, frame.size(), stdout);
        std::fflush(stdout);
    }
    void run_loop(EventLoop& loop) override {
        loop.run();
        present();
    }

private:
    BitmapCanvas canvas_;
    termchart::ColorDepth depth_;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_BACKEND_TK_TUI_BACKEND_HPP
