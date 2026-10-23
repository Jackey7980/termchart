// showcase/panorama_gui.cpp —— TermChart 能力全景 · 真 GUI 窗口版（Win32 后端）
//
// 与 panorama_tui 同一套复杂数据（showcase::build），但渲染走 GUI 平行栈的真实
// 三段（示例内不重写任何光栅化）：
//   chart4d::wire4d / chart3d::scatter3d 产 Scene
//        ─► termchart::gui::SceneRenderer（超采样抗锯齿真彩）
//        ─► image::Bitmap ─► TkCanvas::blit ─► Win32 窗口（GDI StretchDIBits 上屏）
// 布局：上半 4D 超立方体（math4d + chart4d::Projector4D 两阶段投影），
//       下半 Lorenz 吸引子 3D（chart3d）。交互：拖拽轨道 / 滚轮缩放 / 空格暂停 /
//       R 复位 / ESC 退出。--png 无头出图（CI 可验证，无需桌面）。
//
// 说明：仅在 TERMCHART_BUILD_GUI_WIN32=ON 且 Windows 下构建（见 showcase/CMakeLists.txt）。
#ifdef TERMCHART_BUILD_GUI_WIN32

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "termchart/chart3d/common.hpp"
#include "termchart/chart3d/projector.hpp"
#include "termchart/chart3d/scatter.hpp"
#include "termchart/chart4d/projector4d.hpp"
#include "termchart/chart4d/wire.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/gui/font.hpp"
#include "termchart/gui/scene_renderer.hpp"
#include "termchart/image/bitmap.hpp"
#include "termchart/image/png_encode.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart/math3d/vec3.hpp"
#include "termchart/math4d/camera4.hpp"
#include "termchart/math4d/mat5.hpp"
#include "termchart/math4d/vec4.hpp"
#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/native_loop/event.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/mount.hpp"
#include "termchart_gui/widget_tree/widget.hpp"
#include "termchart_gui_win32/win32_backend.hpp"

#include "panorama_data.hpp"

namespace {

using termchart::Point;
using termchart::Rect;
using termchart::Rgb;
using termchart::image::Bitmap;

termchart::gui::RenderOptions make_ro(double width, double height, int ss) {
    termchart::gui::RenderOptions ro;
    ro.width = static_cast<std::uint32_t>(std::lround(width));
    ro.height = static_cast<std::uint32_t>(std::lround(height));
    ro.autofit = false;
    ro.scale = 1.0;
    ro.supersample = ss;
    ro.transparent = false;
    ro.background = Rgb{14, 16, 24};
    ro.text_cell_w = 6.0;
    ro.text_cell_h = 8.0;
    return ro;
}

termchart::Style title_style() {
    termchart::Style s;
    return s.set_fg(termchart::Color::rgb(Rgb{235, 235, 235}));
}
termchart::Style hint_style() {
    termchart::Style s;
    return s.set_fg(termchart::Color::rgb(Rgb{150, 150, 160}));
}

// ── 4D 超立方体 ─────────────────────────────────────────────────────────────
struct HypercubeState {
    double a_xw = 0.55;
    double a_yz = 0.35;
    double azimuth = 32.0;
    double elevation = 20.0;
    double zoom = 1.5;
};

std::vector<termchart::math4d::Vec4> hypercube_vertices() {
    std::vector<termchart::math4d::Vec4> v;
    v.reserve(16);
    for (int i = 0; i < 16; ++i) {
        v.push_back(termchart::math4d::Vec4{(i & 1) ? 1.0 : -1.0, (i & 2) ? 1.0 : -1.0,
                                            (i & 4) ? 1.0 : -1.0, (i & 8) ? 1.0 : -1.0});
    }
    return v;
}
std::vector<termchart::chart4d::Edge> hypercube_edges() {
    std::vector<termchart::chart4d::Edge> e;
    for (int i = 0; i < 16; ++i) {
        for (int b = 0; b < 4; ++b) {
            const int j = i ^ (1 << b);
            if (j > i) e.push_back({static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(j)});
        }
    }
    return e;
}

Bitmap render_hypercube_frame(double width, double height, const HypercubeState& st, int ss) {
    const auto verts = hypercube_vertices();
    const auto edges = hypercube_edges();
    const auto cam = termchart::chart3d::make_camera(width, height, st.azimuth, st.elevation, 55.0,
                                                     termchart::math3d::Vec3{0, 0, 0}, st.zoom);
    const auto inner = std::make_shared<termchart::chart3d::Projector3D>(cam);
    termchart::math4d::Camera4 cam4;
    cam4.distance = 3.0;
    cam4.focal = termchart::math4d::focal_from_fov_deg(60.0);
    const termchart::chart4d::Projector4D projector(
        termchart::math4d::rotate_xw(st.a_xw) * termchart::math4d::rotate_yz(st.a_yz), cam4, inner);
    termchart::chart4d::Wire4dOpts o;
    o.cols = width;
    o.rows = height;
    o.point_size = std::max(3.0, width / 180.0);
    o.edge_width = 2.0;
    o.draw_vertices = true;
    auto scene = termchart::chart4d::wire4d(verts, edges, o, projector);
    {
        termchart::scene::SceneBuilder b(scene);
        b.text(Point{14.0, 12.0}, "四维超立方体 · 两阶段投影", title_style());
    }
    termchart::gui::SceneRenderer renderer(make_ro(width, height, ss));
    return renderer.render(scene);
}

// ── Lorenz 吸引子 3D（同一份复杂数据的 3D 几何）─────────────────────────────
struct LorenzState {
    double azimuth = 35.0;
    double elevation = 18.0;
    double zoom = 48.0;
};

Bitmap render_lorenz_frame(double width, double height, const LorenzState& st, int ss) {
    static const std::vector<termchart::math3d::Vec3> pts = showcase::build(20261022u).lorenz;
    std::vector<termchart::math3d::Vec3> sub;
    sub.reserve(pts.size() / 2);
    for (std::size_t i = 0; i < pts.size(); i += 2) sub.push_back(pts[i]);
    termchart::chart3d::Scatter3dOpts o;
    o.cols = width;
    o.rows = height;
    o.azimuth_deg = st.azimuth;
    o.elevation_deg = st.elevation;
    o.fovy_deg = 55.0;
    o.point_size = std::max(1.5, width / 320.0);
    o.axes = false;
    const auto cam = termchart::chart3d::make_camera(width, height, st.azimuth, st.elevation, 55.0,
                                                     termchart::math3d::Vec3{0, 0, 0}, st.zoom);
    termchart::chart3d::Projector3D proj(cam);
    auto scene = termchart::chart3d::scatter3d(sub, o, &proj);
    {
        termchart::scene::SceneBuilder b(scene);
        b.text(Point{14.0, 12.0}, "Lorenz 吸引子 · chart3d", title_style());
        b.text(Point{14.0, height - 24.0}, "拖拽轨道 · 滚轮缩放 · 空格暂停 · R 复位 · ESC 退出",
               hint_style());
    }
    termchart::gui::SceneRenderer renderer(make_ro(width, height, ss));
    return renderer.render(scene);
}

// 通用轨道交互 + 脏标记（4D 与 3D 共用）。
template <typename State>
bool orbit_event(State& st, const termchart_gui::Event& e, double zoom_lo, double zoom_hi) {
    static thread_local bool dragging = false;  // NOLINT
    static thread_local double lx = 0.0;        // NOLINT
    static thread_local double ly = 0.0;        // NOLINT
    switch (e.type) {
        case termchart_gui::EventType::MousePress:
            if (e.button == 0) {
                dragging = true;
                lx = e.x;
                ly = e.y;
                return true;
            }
            return false;
        case termchart_gui::EventType::MouseRelease:
            if (e.button == 0 && dragging) {
                dragging = false;
                return true;
            }
            return false;
        case termchart_gui::EventType::MouseMove:
            if (dragging) {
                st.azimuth += (e.x - lx) * 0.5;
                st.elevation = std::clamp(st.elevation - (e.y - ly) * 0.5, -89.0, 89.0);
                lx = e.x;
                ly = e.y;
                return true;
            }
            return false;
        case termchart_gui::EventType::Wheel:
            st.zoom *= (e.wheel_delta > 0.0) ? 0.9 : 1.1;
            st.zoom = std::clamp(st.zoom, zoom_lo, zoom_hi);
            return true;
        default:
            return false;
    }
}

class Hypercube4DWidget final : public termchart_gui::Widget {
public:
    Hypercube4DWidget(std::string id, int supersample) : Widget(std::move(id)), ss_(supersample) {}
    void tick() {
        if (!auto_rotate_) return;
        st_.a_xw += 0.030;
        st_.a_yz += 0.019;
        dirty_ = true;
    }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    void paint(termchart_gui::TkCanvas& canvas) const override {
        const Rect g = global_bounds();
        if (g.w < 1.0 || g.h < 1.0) return;
        if (dirty_ || fw_ != g.w || fh_ != g.h) {
            frame_ = render_hypercube_frame(g.w, g.h, st_, ss_);
            fw_ = g.w;
            fh_ = g.h;
            dirty_ = false;
        }
        canvas.blit(frame_, g.x, g.y);
    }
    bool on_event(const termchart_gui::Event& e) override {
        if (orbit_event(st_, e, 0.7, 6.0)) {
            dirty_ = true;
            return true;
        }
        if (e.type == termchart_gui::EventType::KeyPress) {
            if (e.key == ' ') {
                auto_rotate_ = !auto_rotate_;
                dirty_ = true;
                return true;
            }
            if (e.key == 'r' || e.key == 'R') {
                st_ = HypercubeState{};
                dirty_ = true;
                return true;
            }
        }
        return false;
    }

private:
    HypercubeState st_;
    int ss_;
    bool auto_rotate_ = true;
    mutable Bitmap frame_{};
    mutable double fw_ = 0.0;
    mutable double fh_ = 0.0;
    mutable bool dirty_ = true;
};

class Lorenz3DWidget final : public termchart_gui::Widget {
public:
    Lorenz3DWidget(std::string id, int supersample) : Widget(std::move(id)), ss_(supersample) {}
    void tick() {
        if (!auto_rotate_) return;
        st_.azimuth += 0.25;
        dirty_ = true;
    }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    void paint(termchart_gui::TkCanvas& canvas) const override {
        const Rect g = global_bounds();
        if (g.w < 1.0 || g.h < 1.0) return;
        if (dirty_ || fw_ != g.w || fh_ != g.h) {
            frame_ = render_lorenz_frame(g.w, g.h, st_, ss_);
            fw_ = g.w;
            fh_ = g.h;
            dirty_ = false;
        }
        canvas.blit(frame_, g.x, g.y);
    }
    bool on_event(const termchart_gui::Event& e) override {
        if (orbit_event(st_, e, 12.0, 200.0)) {
            dirty_ = true;
            return true;
        }
        if (e.type == termchart_gui::EventType::KeyPress &&
            (e.key == 'r' || e.key == 'R')) {
            st_ = LorenzState{};
            dirty_ = true;
            return true;
        }
        return false;
    }

private:
    LorenzState st_;
    int ss_;
    bool auto_rotate_ = true;
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
    std::uint32_t width = 980;
    std::uint32_t height = 680;
    int supersample = 2;
    std::string png;
    std::string shot;
};

void usage() {
    std::fprintf(stderr,
                 "usage: panorama_gui [--png out.png] [--shot out.png] [--size WxH] [--ss N]\n"
                 "  drag=orbit  wheel=zoom  space=pause  R=reset  ESC=quit\n");
}

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
        if (arg == "--png") {
            opt.png = value();
        } else if (arg == "--shot") {
            opt.shot = value();
        } else if (arg == "--size") {
            const std::string v = value();
            unsigned w = 0;
            unsigned h = 0;
            if (std::sscanf(v.c_str(), "%ux%u", &w, &h) != 2 || w < 64 || h < 64) {
                std::fprintf(stderr, "--size 需要 WxH（>=64），收到 %s\n", v.c_str());
                return 2;
            }
            opt.width = w;
            opt.height = h;
        } else if (arg == "--ss") {
            opt.supersample = std::clamp(std::atoi(value().c_str()), 1, 4);
        } else if (arg == "--help" || arg == "-h") {
            usage();
            return 0;
        } else {
            std::fprintf(stderr, "未知参数 %s\n", arg.c_str());
            usage();
            return 2;
        }
    }

    // 无头：渲染 4D 超立方体一帧存 PNG（CI 可验证，无需桌面）。
    if (!opt.png.empty()) {
        const double top_h = static_cast<double>(opt.height) * 0.55;
        const Bitmap frame =
            render_hypercube_frame(static_cast<double>(opt.width), top_h, HypercubeState{},
                                   opt.supersample);
        if (!save_png(frame, opt.png)) {
            std::fprintf(stderr, "写 PNG 失败：%s\n", opt.png.c_str());
            return 1;
        }
        std::fprintf(stderr, "panorama gui: %ux%u ss=%d -> %s\n", opt.width, opt.height,
                     opt.supersample, opt.png.c_str());
        return 0;
    }

    termchart_gui_win32::Win32Backend backend(opt.width, opt.height, "termchart - panorama 4D/3D");
    if (!backend.valid()) {
        std::fprintf(stderr, "Win32Backend: 无法创建窗口（需要交互桌面会话）；可用 --png 无头出图\n");
        return 2;
    }

    termchart_gui::Container root("root");
    auto& cube = root.emplace_child<Hypercube4DWidget>("cube", opt.supersample);
    auto& lorenz = root.emplace_child<Lorenz3DWidget>("lorenz", opt.supersample);
    const auto layout = [&](std::uint32_t w, std::uint32_t h) {
        const Rect full{0, 0, static_cast<double>(w), static_cast<double>(h)};
        const double split = static_cast<double>(h) * 0.55;
        root.set_bounds(full);
        cube.set_bounds(Rect{0, 0, static_cast<double>(w), split});
        lorenz.set_bounds(Rect{0, split, static_cast<double>(w), static_cast<double>(h) - split});
    };
    layout(opt.width, opt.height);

    termchart_gui::Mount mount(root, backend);
    mount.paint_all();
    backend.set_render_hook([&](std::uint32_t w, std::uint32_t h) {
        layout(w, h);
        mount.paint_all();
    });

    if (!opt.shot.empty()) {
        if (!save_png(backend.canvas().snapshot(), opt.shot)) {
            std::fprintf(stderr, "写快照失败：%s\n", opt.shot.c_str());
            backend.request_close();
            return 1;
        }
        std::fprintf(stderr, "窗口首帧已存至 %s\n", opt.shot.c_str());
        backend.request_close();
        for (int i = 0; i < 50; ++i) {
            termchart_gui::Event e;
            while (backend.poll_event(e)) {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
        return 0;
    }

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
        cube.tick();
        lorenz.tick();
        if (cube.dirty() || lorenz.dirty()) mount.paint_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    backend.request_close();
    return 0;
}

#else
#include <cstdio>
int main() {
    std::fprintf(stderr, "panorama_gui 需要 TERMCHART_BUILD_GUI_WIN32=ON 且 Windows 平台\n");
    return 2;
}
#endif
