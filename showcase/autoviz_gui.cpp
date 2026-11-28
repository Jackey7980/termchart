// showcase/autoviz_gui.cpp —— 让 autoviz 自己"选对图"，复用 Panorama 难数据，真 GUI 窗口版。
//
// 与 autoviz_tui 同一套逻辑（showcase::run_autoviz 选图），但渲染走 GUI 平行栈：
//   api::chart_to_scene(chart, 字符列, 字符行) → scene::Scene（场景单位即字符格）
//        ─► termchart::gui::SceneRenderer（超采样抗锯齿真彩，cell→像素按 text_cell_w/h）
//        ─► image::Bitmap ─► TkCanvas::blit ─► Win32 窗口（GDI StretchDIBits 上屏）
// 交互：滚轮缩放 / R 复位 / ESC 退出；--png 无头出图（CI 可验证，无需桌面）。
//
// 说明：仅在 TERMCHART_BUILD_GUI_WIN32=ON 且 Windows 下构建（见 showcase/CMakeLists.txt）。
#ifdef TERMCHART_BUILD_GUI_WIN32

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <chrono>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/gui/scene_renderer.hpp"
#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/png_encode.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart/api/chart.hpp"

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/native_loop/event.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/mount.hpp"
#include "termchart_gui/widget_tree/widget.hpp"
#include "termchart_gui_win32/win32_backend.hpp"

#include "autoviz_common.hpp"

namespace {

using termchart::Point;
using termchart::Rect;
using termchart::Rgb;
using termchart::image::Bitmap;

// 场景坐标 = 字符格。SceneRenderer 的映射是 1 场景单位 = scale 像素，字形 advance =
// text_cell_w × scale。要"1 格 = 1 字形"必须 text_cell = 1×1，再用 scale = 每格像素数 S
// 放大：cols×S = 窗口宽、rows×S = 窗口高，图精确铺满且字号 = S 像素（清晰可读）。
termchart::gui::RenderOptions make_ro(double width, double height, int ss, double scale) {
    termchart::gui::RenderOptions ro;
    ro.width = static_cast<std::uint32_t>(std::lround(width));
    ro.height = static_cast<std::uint32_t>(std::lround(height));
    ro.autofit = false;
    ro.scale = scale;
    ro.supersample = ss;
    ro.transparent = false;
    ro.background = Rgb{14, 16, 24};
    ro.text_cell_w = 1.0;
    ro.text_cell_h = 1.0;
    return ro;
}

termchart::Style hint_style() {
    termchart::Style s;
    return s.set_fg(termchart::Color::rgb(Rgb{150, 150, 160}));
}

// 把 autoviz 选中的 chart 渲染成 Bitmap：cols/rows 按窗口像素 ÷ 每格像素(cell_px) 反推，
// scale = cell_px × zoom → 内容铺满整个窗口，字号 = cell_px 像素。底部留 2 行作状态条，
// 避免和图表自身的 x 轴标签重叠。
Bitmap render_chart_frame(double width, double height, const termchart::chart::Chart& chart,
                          double zoom, int ss, int cell_px, const std::string& meta) {
    const double s = static_cast<double>(std::max(6, cell_px));
    const std::uint32_t cols =
        std::max<std::uint32_t>(8u, static_cast<std::uint32_t>(std::lround(width / s)));
    const std::uint32_t rows = std::max<std::uint32_t>(
        8u, static_cast<std::uint32_t>(std::lround(height / s)) - 2u);
    auto res = termchart::api::chart_to_scene(chart, cols, rows);
    termchart::scene::Scene scene = res ? std::move(*res) : termchart::scene::Scene{};
    {
        termchart::scene::SceneBuilder b(scene);
        b.text(Point{1.0, static_cast<double>(rows) + 1.0}, meta, hint_style());
    }
    termchart::gui::RenderOptions ro = make_ro(width, height, ss, s * zoom);
    termchart::gui::SceneRenderer renderer(ro);
    return renderer.render(scene);
}

class AutovizWidget final : public termchart_gui::Widget {
public:
    // 不缓存 rows/cols：每帧按当前窗口像素 ÷ 每格像素反推，图随窗口铺满。
    AutovizWidget(std::string id, const termchart::chart::Chart& chart, const std::string& meta,
                  int supersample, int cell_px)
        : Widget(std::move(id)), chart_(chart), meta_(meta), ss_(supersample), cell_(cell_px) {}

    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    void paint(termchart_gui::TkCanvas& canvas) const override {
        const Rect g = global_bounds();
        if (g.w < 1.0 || g.h < 1.0) return;
        if (dirty_ || fw_ != g.w || fh_ != g.h) {
            frame_ = render_chart_frame(g.w, g.h, chart_, zoom_, ss_, cell_, meta_);
            fw_ = g.w;
            fh_ = g.h;
            dirty_ = false;
        }
        canvas.blit(frame_, g.x, g.y);
    }
    bool on_event(const termchart_gui::Event& e) override {
        if (e.type == termchart_gui::EventType::Wheel) {
            zoom_ *= (e.wheel_delta > 0.0) ? 1.1 : 0.9;
            zoom_ = std::clamp(zoom_, 0.5, 8.0);
            dirty_ = true;
            return true;
        }
        if (e.type == termchart_gui::EventType::KeyPress &&
            (e.key == 'r' || e.key == 'R')) {
            zoom_ = 1.0;
            dirty_ = true;
            return true;
        }
        return false;
    }

private:
    const termchart::chart::Chart& chart_;
    std::string meta_;
    int ss_;
    int cell_;
    double zoom_ = 1.0;
    mutable Bitmap frame_{};
    mutable double fw_ = 0.0;
    mutable double fh_ = 0.0;
    mutable bool dirty_ = true;
};

bool save_png(const Bitmap& bitmap, const std::string& path) {
    const std::string bytes = termchart::image::encode_png(bitmap);
    if (bytes.empty()) return false;
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const std::size_t n = std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return n == bytes.size();
}

struct Options {
    std::uint32_t width = 1000;
    std::uint32_t height = 720;
    int supersample = 2;
    int cell_px = 12;  // 每个字符格的像素边长（字号），越大字越清晰、格数越少
    std::string png;
};

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto value = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s: 缺少参数值\n", arg.c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--png") opt.png = value();
        else if (arg == "--size") {
            unsigned w = 0, h = 0;
            if (std::sscanf(value().c_str(), "%ux%u", &w, &h) != 2 || w < 64 || h < 64) {
                std::fprintf(stderr, "--size 需要 WxH（>=64）\n");
                return 2;
            }
            opt.width = w;
            opt.height = h;
        } else if (arg == "--ss") {
            opt.supersample = std::clamp(std::atoi(value().c_str()), 1, 4);
        } else if (arg == "--cell") {
            opt.cell_px = std::clamp(std::atoi(value().c_str()), 6, 32);
        } else if (arg == "--help" || arg == "-h") {
            std::fprintf(stderr,
                "usage: autoviz_gui [--png out.png] [--size WxH] [--ss N] [--cell N]\n"
                "  cell=每格像素(字号,默认12)  wheel=zoom  R=reset  ESC=quit\n");
            return 0;
        } else {
            std::fprintf(stderr, "未知参数 %s\n", arg.c_str());
            return 2;
        }
    }

    const showcase::AvResult r = showcase::run_autoviz();
    if (!r.error.empty() || !r.chart) {
        std::fprintf(stderr, "autoviz 失败: %s\n", r.error.c_str());
        return 1;
    }
    const std::string meta = "autoviz: " + r.name + " | 管道=" + r.pipeline +
                             " | 滚轮缩放 R 复位 ESC 退出";

    // 无头：渲染一帧存 PNG（CI 可验证，无需桌面）。
    if (!opt.png.empty()) {
        const Bitmap frame =
            render_chart_frame(static_cast<double>(opt.width), static_cast<double>(opt.height),
                               *r.chart, 1.0, opt.supersample, opt.cell_px, meta);
        if (!save_png(frame, opt.png)) {
            std::fprintf(stderr, "写 PNG 失败：%s\n", opt.png.c_str());
            return 1;
        }
        std::fprintf(stderr, "autoviz gui: %s -> %s\n", r.name.c_str(), opt.png.c_str());
        return 0;
    }

    termchart_gui_win32::Win32Backend backend(opt.width, opt.height,
                                              ("termchart - autoviz: " + r.name).c_str());
    if (!backend.valid()) {
        std::fprintf(stderr, "Win32Backend: 无法创建窗口（需要交互桌面会话）；可用 --png 无头出图\n");
        return 2;
    }

    termchart_gui::Container root("root");
    auto& av = root.emplace_child<AutovizWidget>("autoviz", *r.chart, meta, opt.supersample,
                                                 opt.cell_px);
    const auto layout = [&](std::uint32_t w, std::uint32_t h) {
        root.set_bounds(Rect{0, 0, static_cast<double>(w), static_cast<double>(h)});
    };
    layout(opt.width, opt.height);

    termchart_gui::Mount mount(root, backend);
    mount.paint_all();
    backend.set_render_hook([&](std::uint32_t w, std::uint32_t h) {
        layout(w, h);
        mount.paint_all();
    });

    bool running = true;
    while (running) {
        termchart_gui::Event e;
        while (backend.poll_event(e)) {
            switch (e.type) {
                case termchart_gui::EventType::Quit:
                    running = false;
                    break;
                case termchart_gui::EventType::Resize:
                    layout(e.width, e.height);
                    break;
                default:
                    (void)mount.dispatch_event(e);
                    break;
            }
        }
        if (av.dirty()) mount.paint_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    backend.request_close();
    return 0;
}

#else
#include <cstdio>
int main() {
    std::fprintf(stderr, "autoviz_gui 需要 TERMCHART_BUILD_GUI_WIN32=ON 且 Windows 平台\n");
    return 2;
}
#endif
