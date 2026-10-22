// 81b_gui_only.cpp —— 临时示例：纯 Win32 真窗口 GUI，零回退（没有 TuiBackend 终端分支）。
//
// 数据 / 场景生成逻辑与 81_spatiotemporal_network_demo.cpp 完全同源（独立副本）。
// 本文件刻意只用 termchart_gui_win32 的 Win32Backend 开真实窗口；若未启用
// TERMCHART_BUILD_GUI_WIN32 插件则直接 #error——明确"没有回退到终端"。
//
// 跑法（需在本机有桌面的 Windows 会话里、且用 -DTERMCHART_BUILD_GUI_WIN32=ON 构建）：
//   .\build\ninja\examples\81b_gui_only.exe
// 顶部工具栏 [+] / [-] 点击缩放，下方画布占满剩余空间。ESC / 关闭按钮退出。

#ifndef TERMCHART_BUILD_GUI_WIN32
#error "81b_gui_only requires TERMCHART_BUILD_GUI_WIN32=ON (real Win32 window; no terminal fallback)"
#endif

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "termchart/core/buffer.hpp"
#include "termchart/core/style.hpp"
#include "termchart/core/attr.hpp"
#include "termchart/math3d/vec3.hpp"
#include "termchart/math3d/camera.hpp"
#include "termchart/chart3d/common.hpp"        // chart3d::make_camera / shade
#include "termchart/scene/scene.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/interaction/zoom.hpp"       // interaction::Viewport
#include "termchart/gui/scene_renderer.hpp"     // gui::SceneRenderer（真线宽）
#include "termchart/image/bitmap.hpp"           // image::Bitmap

#include "termchart_gui/gui.hpp"
#include "termchart_gui/widget_tree/button.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/layout/flex.hpp"
#include "termchart_gui/backend_tk/backend.hpp" // Mount / Widget / Event / EventType / TkCanvas
#include "termchart_gui_win32/win32_backend.hpp"

namespace demo {

using namespace termchart;

// ── 数据模型（硬编码）────────────────────────────────────────────────────
struct Node { std::string id; math3d::Vec3 pos; };
struct Edge {
    int from = 0;
    int to = 0;
    double flow = 0.0;     // → 线宽
    std::string weather;   // → 颜色
};
struct Network { std::vector<Node> nodes; std::vector<Edge> edges; };

Network make_network() {
    Network n;
    n.nodes = {
        {"N01", {0.0, 0.0, 0.0}},
        {"N02", {100.0, 50.0, 30.0}},
        {"N03", {-80.0, 60.0, 20.0}},
        {"N04", {20.0, -90.0, 40.0}},
        {"N05", {-30.0, -50.0, 10.0}},
        {"N06", {60.0, 80.0, 50.0}},
    };
    n.edges = {
        {0, 1, 35.0, "clear"},
        {0, 2, 28.0, "cloudy"},
        {0, 3, 42.0, "rain"},
        {1, 5, 20.0, "clear"},
        {2, 4, 15.0, "cloudy"},
        {3, 4, 33.0, "rain"},
        {1, 2, 18.0, "clear"},
        {4, 5, 25.0, "cloudy"},
        {3, 5, 12.0, "clear"},
        {2, 5, 22.0, "rain"},
    };
    return n;
}

Color weather_color(const std::string& w) {
    if (w == "rain") return Color::rgb(70, 130, 230);
    if (w == "cloudy") return Color::rgb(230, 200, 60);
    return Color::rgb(60, 200, 90);  // clear
}

double flow_width(double flow) {
    const double w = 1.0 + (flow - 10.0) / 8.0;
    return w < 1.0 ? 1.0 : (w > 5.0 ? 5.0 : w);
}

void bounds(const Network& net, math3d::Vec3& center, double& radius) {
    math3d::Vec3 mn = net.nodes.front().pos, mx = net.nodes.front().pos;
    for (const auto& nd : net.nodes) {
        mn.x = std::min(mn.x, nd.pos.x); mx.x = std::max(mx.x, nd.pos.x);
        mn.y = std::min(mn.y, nd.pos.y); mx.y = std::max(mx.y, nd.pos.y);
        mn.z = std::min(mn.z, nd.pos.z); mx.z = std::max(mx.z, nd.pos.z);
    }
    center = (mn + mx) * 0.5;
    radius = (mx - mn).length() * 0.5;
    if (!(radius > 0.0)) radius = 1.0;
}

// ── L12：生成 Scene（3D→2D）。GUI 路径走真实线宽（faux_width=false）。────
scene::Scene build_scene(const Network& net, int cols, int rows,
                         const interaction::Viewport& vp) {
    scene::Scene s;
    math3d::Vec3 center;
    double radius = 0.0;
    bounds(net, center, radius);

    const math3d::Camera cam = chart3d::make_camera(
        static_cast<double>(cols), static_cast<double>(rows),
        35.0, 22.0, 55.0, center, radius);

    const auto map_pt = [&](const math3d::Projected& p) -> Point {
        return Point{
            vp.normalized_x(p.x) * static_cast<double>(cols),
            vp.normalized_y(p.y) * static_cast<double>(rows),
        };
    };

    for (const auto& e : net.edges) {
        const auto pa = cam.project(net.nodes[static_cast<std::size_t>(e.from)].pos,
                                    static_cast<double>(cols), static_cast<double>(rows));
        const auto pb = cam.project(net.nodes[static_cast<std::size_t>(e.to)].pos,
                                    static_cast<double>(cols), static_cast<double>(rows));
        if (!pa.visible || !pb.visible) continue;
        const Point a = map_pt(pa);
        const Point b = map_pt(pb);
        const Color c = weather_color(e.weather);
        const double w = flow_width(e.flow);
        s.push(scene::StrokeOp{c, w});          // GUI 真正尊重线宽
        s.push(scene::MoveTo{Point{a.x, a.y}});
        s.push(scene::LineTo{Point{b.x, b.y}});
    }

    for (const auto& nd : net.nodes) {
        const auto p = cam.project(nd.pos, static_cast<double>(cols), static_cast<double>(rows));
        if (!p.visible) continue;
        const Point q = map_pt(p);
        s.push(scene::FillOp{chart3d::shade(p.depth)});
        s.push(scene::RectOp{Rect{q.x - 0.45, q.y - 0.45, 0.9, 0.9}});
        s.push(scene::TextOp{Point{q.x + 0.6, q.y - 0.3}, nd.id, Style{}});
    }
    return s;
}

// ── GUI 画布控件 ────────────────────────────────────────────────────────
namespace gui_demo {

using namespace termchart_gui;

class NetworkCanvas : public Widget {
public:
    static constexpr int kCellsX = 200, kCellsY = 90;  // 离屏「单元格」视口

    NetworkCanvas(std::string id, const Network& net)
        : Widget(std::move(id)), net_(&net),
          viewport_(Rect{0.0, 0.0, static_cast<double>(kCellsX),
                         static_cast<double>(kCellsY)},
                    /*min_span=*/4.0) {}

    interaction::Viewport& viewport() noexcept { return viewport_; }

    void paint(TkCanvas& c) const override {
        const Rect g = global_bounds();
        c.clear(Rgb{16, 18, 24});
        const int w = static_cast<int>(g.w), h = static_cast<int>(g.h);
        if (w <= 0 || h <= 0) return;
        // 真实线宽 + autofit 把单元格坐标缩放到 w×h 像素位图。
        const scene::Scene s = build_scene(*net_, kCellsX, kCellsY, viewport_);
        termchart::gui::SceneRenderer sr{termchart::gui::RenderOptions{
            .width = static_cast<std::uint32_t>(w),
            .height = static_cast<std::uint32_t>(h),
            .autofit = true,
            .background = Rgb{16, 18, 24},
        }};
        const termchart::image::Bitmap bmp = sr.render(s);
        c.blit(bmp, g.x, g.y);  // TODO: 缺箭头 API —— 方向仅由连线顺序表达
        c.draw_text(g.x + 6.0, g.y + 6.0,
                    "scale=" + std::to_string(viewport_.scale_x()),
                    Rgb{220, 220, 226});
    }

private:
    const Network* net_ = nullptr;
    interaction::Viewport viewport_;
};

}  // namespace gui_demo

// 纯 Win32 真窗口 GUI（无回退）。
int run_gui() {
    using namespace termchart_gui;
    const int W = 480, H = 320;

    termchart_gui_win32::Win32Backend backend(W, H, "spatiotemporal network");
    if (!backend.valid()) {
        std::fprintf(stderr, "Win32 backend unavailable (no interactive desktop?)\n");
        return 2;
    }

    const Network net = make_network();
    Container root("root");
    root.set_bounds(Rect{0.0, 0.0, static_cast<double>(W), static_cast<double>(H)});

    auto& toolbar = root.emplace_child<Container>("toolbar");
    toolbar.set_vpolicy(SizePolicy::Fixed);
    toolbar.set_size(static_cast<double>(W), 36.0);
    auto fl = std::make_unique<FlexLayout>();
    fl->direction = FlexDirection::Row;
    fl->gap = 6.0;
    fl->cross_align = FlexAlign::Stretch;
    toolbar.set_layout(std::move(fl));
    auto& plus = toolbar.emplace_child<Button>("plus", "+");
    auto& minus = toolbar.emplace_child<Button>("minus", "-");

    auto& canvas = root.emplace_child<gui_demo::NetworkCanvas>("canvas", net);

    Mount mount(root, backend);
    const auto db = canvas.viewport().data_bounds();
    const Point center{db.x + db.w / 2.0, db.y + db.h / 2.0};
    plus.set_on_click([&] { canvas.viewport().zoom_at(1.1, center); mount.paint_all(); });
    minus.set_on_click([&] { canvas.viewport().zoom_at(1.0 / 1.1, center); mount.paint_all(); });

    mount.paint_all();  // 首帧

    auto drain = [&](bool& running) -> bool {
        bool dirty = false;
        Event e;
        while (backend.poll_event(e)) {
            if (e.type == EventType::Quit) { running = false; return dirty; }
            (void)mount.dispatch_event(e);  // 命中按钮 → on_click → 缩放 → 重绘
            dirty = true;
        }
        return dirty;
    };
    bool running = true;
    while (running) {
        if (drain(running)) mount.paint_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    return 0;
}

}  // namespace demo

int main() {
    return demo::run_gui();
}
