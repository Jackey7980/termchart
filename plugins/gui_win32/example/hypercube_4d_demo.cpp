// hypercube_4d_demo.cpp —— 4D 超立方体的真 GUI 窗口版（Win32 后端）
//
// 与终端版 examples/74 同一套 4D 数学（math4d + chart4d::Projector4D 两阶段投影），
// 但渲染走 GUI 平行栈的真实三段（示例内不重写任何光栅化）：
//   chart4d::wire4d 产 Scene ─► termchart::gui::SceneRenderer（超采样抗锯齿真彩）
//        ─► image::Bitmap ─► TkCanvas::blit ─► Win32 窗口（GDI StretchDIBits 上屏）
// 交互：拖拽 = 轨道旋转（方位角/仰角）；滚轮 = 缩放；空格 = 暂停/继续自转；R = 复位；
//       ESC / 关闭按钮退出。自转由主循环每帧 tick 推进（XW + YZ 两平面）。
//
// 用法：
//   hypercube_4d_demo                  开窗口交互（ESC / 关闭按钮退出）
//   hypercube_4d_demo --png out.png    无头渲染一帧存 PNG（不需要窗口/桌面，CI 可用）
//   hypercube_4d_demo --shot out.png   开窗口、首帧存快照后退出
//   hypercube_4d_demo --size 900x620   初始窗口尺寸
//   hypercube_4d_demo --ss 2           超采样倍数（1–4，默认 2）
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "termchart/chart3d/common.hpp"     // chart3d::make_camera
#include "termchart/chart3d/projector.hpp"  // chart3d::Projector3D
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

namespace {

using termchart::Point;
using termchart::Rect;
using termchart::Rgb;
using termchart::image::Bitmap;

// 超立方体的可视状态：4D 自转角 + 3D 轨道角 + 缩放（内层相机半径）。
struct HypercubeState {
    double a_xw = 0.55;    // XW 平面旋转角（弧度）
    double a_yz = 0.35;    // YZ 平面旋转角
    double azimuth = 32.0;   // 3D 轨道方位角（度）
    double elevation = 20.0;  // 3D 轨道仰角（度）
    double zoom = 1.5;        // 内层相机包围半径（越小越近、越大越远；1.5 下全程 32 边不裁）
};

// 16 顶点超立方体：4 个坐标各取 ±1。
std::vector<termchart::math4d::Vec4> hypercube_vertices() {
    std::vector<termchart::math4d::Vec4> v;
    v.reserve(16);
    for (int i = 0; i < 16; ++i) {
        v.push_back(termchart::math4d::Vec4{(i & 1) ? 1.0 : -1.0, (i & 2) ? 1.0 : -1.0,
                                            (i & 4) ? 1.0 : -1.0, (i & 8) ? 1.0 : -1.0});
    }
    return v;
}

// 32 条边：仅连二进制位相差 1 的顶点对（4-正则，16×4/2=32）。
std::vector<termchart::chart4d::Edge> hypercube_edges() {
    std::vector<termchart::chart4d::Edge> e;
    for (int i = 0; i < 16; ++i) {
        for (int b = 0; b < 4; ++b) {
            const int j = i ^ (1 << b);
            if (j > i) {
                e.push_back({static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(j)});
            }
        }
    }
    return e;
}

// 把一帧渲染成 RGBA 位图：Scene 是"画什么"，SceneRenderer 是"怎么落成像素"。
// 与 panda_demo 同构——autofit=false 让 wire4d 的画布坐标即像素坐标。
Bitmap render_hypercube_frame(double width, double height, const HypercubeState& st, int ss) {
    const auto verts = hypercube_vertices();
    const auto edges = hypercube_edges();

    // 阶段二：内层 3D 轨道相机（拖拽改 azimuth/elevation，滚轮改 zoom）。
    const auto cam = termchart::chart3d::make_camera(
        width, height, st.azimuth, st.elevation, /*fovy*/ 55.0, termchart::math3d::Vec3{0, 0, 0},
        st.zoom);
    const auto inner = std::make_shared<termchart::chart3d::Projector3D>(cam);

    // 阶段一：4D 相机架在 +W 轴，~60° 视场推焦距。
    termchart::math4d::Camera4 cam4;
    cam4.distance = 3.0;
    cam4.focal = termchart::math4d::focal_from_fov_deg(60.0);

    const termchart::chart4d::Projector4D projector(
        termchart::math4d::rotate_xw(st.a_xw) * termchart::math4d::rotate_yz(st.a_yz), cam4, inner);

    termchart::chart4d::Wire4dOpts o;
    o.cols = width;
    o.rows = height;
    o.point_size = std::max(3.0, width / 180.0);  // 顶点方块：随画布缩几像素
    o.edge_width = 2.0;  // GUI 像素画布：2px 抗锯齿边线（终端用默认 1.0）
    o.draw_vertices = true;
    auto scene = termchart::chart4d::wire4d(verts, edges, o, projector);

    termchart::gui::RenderOptions ro;
    ro.width = static_cast<std::uint32_t>(std::lround(width));
    ro.height = static_cast<std::uint32_t>(std::lround(height));
    ro.autofit = false;  // 坐标即像素
    ro.scale = 1.0;
    ro.supersample = ss;
    ro.transparent = false;
    ro.background = Rgb{14, 16, 24};
    ro.text_cell_w = 6.0;
    ro.text_cell_h = 8.0;

    // 中文叠加层：FontBackend 组合链让非 ASCII 码点走系统 TTF（Windows 自动试
    // msyh.ttc/simhei.ttf），ASCII 仍走点阵；SceneBuilder 只持引用不清空，
    // 可直接往 wire4d 的 Scene 上追文本（文本路径按最终分辨率直绘并置顶）。
    {
        using termchart::Color;
        using termchart::Point;
        using termchart::Style;
        termchart::scene::SceneBuilder b(scene);
        Style title;
        title.set_fg(Color::rgb(termchart::Rgb{235, 235, 235}));
        Style hint;
        hint.set_fg(Color::rgb(termchart::Rgb{150, 150, 150}));
        constexpr std::string_view kTitle = "四维超立方体";
        constexpr std::string_view kHint =
            "拖拽旋转 · 滚轮缩放 · 空格暂停 · R 复位 · ESC 退出";
        b.text(Point{14.0, 12.0}, kTitle, title);
        const double hint_w =
            termchart::gui::measure_text(kHint, ro.text_cell_w, ro.text_cell_h).width;
        b.text(Point{width - 14.0 - hint_w, height - ro.text_cell_h - 10.0}, kHint, hint);
    }

    termchart::gui::SceneRenderer renderer(ro);
    return renderer.render(scene);
}

bool save_png(const Bitmap& bitmap, const std::string& path) {
    const std::string bytes = termchart::image::encode_png(bitmap);
    if (bytes.empty()) return false;
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

// 窗口里的超立方体：状态变化（自转 tick / 拖拽 / 滚轮 / 复位）才重渲染，否则 blit 缓存。
class Hypercube4DWidget final : public termchart_gui::Widget {
public:
    Hypercube4DWidget(std::string id, int supersample)
        : Widget(std::move(id)), ss_(supersample) {}

    // 主循环每帧调用：推进自转（XW + YZ），返回本帧是否因自转而变脏。
    void tick() {
        if (!auto_rotate_) return;
        st_.a_xw += 0.030;
        st_.a_yz += 0.019;
        dirty_ = true;
    }

    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    [[nodiscard]] bool auto_rotate() const noexcept { return auto_rotate_; }

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
        switch (e.type) {
            case termchart_gui::EventType::MousePress:
                if (e.button == 0) {
                    dragging_ = true;
                    last_x_ = e.x;
                    last_y_ = e.y;
                    return true;
                }
                return false;
            case termchart_gui::EventType::MouseRelease:
                if (e.button == 0 && dragging_) {
                    dragging_ = false;
                    return true;
                }
                return false;
            case termchart_gui::EventType::MouseMove:
                if (dragging_) {
                    st_.azimuth += (e.x - last_x_) * 0.5;
                    st_.elevation -= (e.y - last_y_) * 0.5;
                    st_.elevation = std::clamp(st_.elevation, -89.0, 89.0);
                    last_x_ = e.x;
                    last_y_ = e.y;
                    dirty_ = true;
                    return true;
                }
                return false;
            case termchart_gui::EventType::Wheel:
                // 上滚（delta>0）拉近 → 包围半径变小。
                st_.zoom *= (e.wheel_delta > 0.0) ? 0.9 : 1.1;
                st_.zoom = std::clamp(st_.zoom, 0.7, 6.0);
                dirty_ = true;
                return true;
            case termchart_gui::EventType::KeyPress:
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
                return false;
            default:
                return false;
        }
    }

private:
    HypercubeState st_;
    int ss_;
    bool auto_rotate_ = true;
    bool dragging_ = false;
    double last_x_ = 0.0;
    double last_y_ = 0.0;
    mutable Bitmap frame_{};
    mutable double fw_ = 0.0;
    mutable double fh_ = 0.0;
    mutable bool dirty_ = true;
};

struct Options {
    std::uint32_t width = 860;
    std::uint32_t height = 620;
    int supersample = 2;
    std::string png;
    std::string shot;
};

void usage() {
    std::fprintf(stderr,
                 "usage: hypercube_4d_demo [--png out.png] [--shot out.png] [--size WxH] [--ss N]\n"
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

    // 无头：不需要窗口/桌面，直接把一帧（默认视角）存成 PNG——CI 可验证。
    if (!opt.png.empty()) {
        const auto t0 = std::chrono::steady_clock::now();
        const Bitmap frame =
            render_hypercube_frame(opt.width, opt.height, HypercubeState{}, opt.supersample);
        const double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                .count();
        if (!save_png(frame, opt.png)) {
            std::fprintf(stderr, "写 PNG 失败：%s\n", opt.png.c_str());
            return 1;
        }
        std::fprintf(stderr, "hypercube 4d: %ux%u ss=%d, %.1f ms -> %s\n", opt.width, opt.height,
                     opt.supersample, ms, opt.png.c_str());
        return 0;
    }

    using termchart_gui::Event;
    using termchart_gui::EventType;

    termchart_gui_win32::Win32Backend backend(opt.width, opt.height, "termchart_gui - hypercube 4D");
    if (!backend.valid()) {
        std::fprintf(stderr, "Win32Backend: 无法创建窗口（需要交互桌面会话）；可用 --png 无头出图\n");
        return 2;
    }

    termchart_gui::Container root("root");
    auto& cube = root.emplace_child<Hypercube4DWidget>("cube", opt.supersample);
    const auto layout = [&](std::uint32_t w, std::uint32_t h) {
        const Rect r{0, 0, static_cast<double>(w), static_cast<double>(h)};
        root.set_bounds(r);
        cube.set_bounds(r);
    };
    layout(opt.width, opt.height);

    termchart_gui::Mount mount(root, backend);
    mount.paint_all();

    // 拖动边框时系统进入模态 resize 循环会卡住主循环；设 render_hook 就地重排+重画。
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
            Event e;
            while (backend.poll_event(e)) {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
        return 0;
    }

    bool running = true;
    while (running) {
        Event e;
        while (backend.poll_event(e)) {
            switch (e.type) {
                case EventType::Quit:
                    running = false;
                    break;
                case EventType::Resize:
                    layout(e.width, e.height);
                    break;
                default:
                    (void)mount.dispatch_event(e);  // → Hypercube4DWidget::on_event
                    break;
            }
        }
        cube.tick();                       // 推进自转（暂停时不变脏）
        if (cube.dirty()) mount.paint_all();  // paint_all 内含 present 上屏
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    backend.request_close();
    return 0;
}
