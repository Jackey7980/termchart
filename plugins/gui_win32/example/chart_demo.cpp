// chart_demo.cpp —— GUI 组合图 + HSI K 线 并排（"万能插座"演示，原生 Win32）
//
// 复用 ChartWidget（万能插座）：左半边是 combo（柱+折线+面积，深色），右半边是
// HsiKLineRenderer 插进去的恒生指数日 K 线（浅色 MSN 风）。两者共用同一套动画/缓存/
// 降分辨率/淡入/悬停十字轴——证明"插座"与"插头"解耦。
//
// 交互：移动鼠标看悬停（轴指针 + 强调 + 信息框）；拖边框看自适应；ESC/关闭退出。
// 零第三方依赖（仅系统 user32/gdi32）。
//
// 跑法：chart_demo            → 开窗口交互
//       chart_demo --smoke    → 无交互冒烟：渲染数帧后自动关闭（供验证）
//       chart_demo --bench W H N → 无头性能基准（combo 单图）
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/gui.hpp"
#include "termchart_gui/layout/flex.hpp"
#include "termchart_gui/widget_tree/chart_widget.hpp"
#include "termchart_gui/widget_tree/combo_renderer.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/hsi_kline_renderer.hpp"
#include "termchart_gui/widget_tree/mount.hpp"
#include "termchart_gui_win32/win32_backend.hpp"

int main(int argc, char** argv) {
    using namespace termchart_gui;

    const std::vector<std::string> kLabels = {"6-4", "6-7", "6-10", "6-13", "6-16",
                                              "6-19", "6-22", "6-25", "6-28", "7-1", "7-2"};
    const std::vector<double> kBars = {120, 200, 150, 260, 210, 300, 240, 280, 190, 230, 260};
    const std::vector<double> kLine = {130, 210, 160, 250, 220, 310, 245, 290, 200, 240, 270};

    // 恒生指数日线（date, open, high, low, close），2026-09-11 ~ 2026-10-16。
    // 来自东方财富历史行情接口（push2his.eastmoney.com, secid=100.HSI, klt=101）。
    const std::vector<std::string> kHsiLabels = {
        "2026-09-11", "2026-09-14", "2026-09-15", "2026-09-16", "2026-09-17",
        "2026-09-18", "2026-09-21", "2026-09-22", "2026-09-23", "2026-09-24",
        "2026-09-25", "2026-09-28", "2026-09-29", "2026-09-30", "2026-10-02",
        "2026-10-05", "2026-10-06", "2026-10-07", "2026-10-08", "2026-10-09",
        "2026-10-12", "2026-10-13", "2026-10-14", "2026-10-15", "2026-10-16"};
    const std::vector<HsiKLineRenderer::Candle> kHsiCandles = {
        {24718.15, 24851.18, 24570.24, 24805.63}, {24701.18, 24926.24, 24662.11, 24917.60},
        {24934.06, 24934.06, 24657.01, 24667.24}, {24789.91, 24789.91, 24574.75, 24713.78},
        {24480.57, 24606.15, 24357.69, 24604.29}, {24723.85, 24862.98, 24703.92, 24750.78},
        {24748.45, 25042.71, 24736.92, 25042.71}, {25170.80, 25254.11, 25043.64, 25087.75},
        {25064.84, 25072.25, 24811.85, 24834.12}, {24649.45, 24791.20, 24648.29, 24761.13},
        {24523.50, 24537.14, 24275.56, 24510.09}, {24554.58, 24767.22, 24554.58, 24642.51},
        {24648.64, 24648.64, 24444.15, 24523.57}, {24393.88, 24637.65, 24332.64, 24613.27},
        {24099.73, 24099.73, 23865.33, 23972.29}, {23963.41, 24040.34, 23835.68, 24040.34},
        {24279.63, 24354.11, 24179.38, 24280.56}, {24172.29, 24266.92, 24071.41, 24130.50},
        {24031.66, 24146.01, 23737.14, 23785.79}, {23941.26, 24224.05, 23941.26, 24211.35},
        {24434.93, 24455.95, 24193.30, 24271.20}, {24172.09, 24345.22, 23987.77, 24345.22},
        {24282.25, 24524.63, 24115.00, 24314.17}, {24119.14, 24162.71, 23945.84, 24062.72},
        {24321.45, 24392.30, 24087.23, 24186.83}};

    // 无头性能基准：不开窗口，用 HeadlessBackend 复现 fullscreen 的建Scene/光栅/合成成本。
    if (argc >= 2 && std::string(argv[1]) == "--bench") {
        const std::uint32_t W =
            (argc >= 3) ? static_cast<std::uint32_t>(std::stoul(argv[2])) : 1920u;
        const std::uint32_t H =
            (argc >= 4) ? static_cast<std::uint32_t>(std::stoul(argv[3])) : 1080u;
        const int frames = (argc >= 5) ? std::stoi(argv[4]) : 30;
        HeadlessBackend hb(W, H);
        Container root("root");
        root.set_bounds(Rect{0, 0, static_cast<double>(W), static_cast<double>(H)});
        auto& chart = root.emplace_child<ChartWidget>("chart");
        chart.set_data(kLabels, kBars, kLine);
        Mount mount(root, hb);
        chart.set_profile(true);
        mount.paint_all();  // 小尺寸先落定一帧
        std::fprintf(stderr, "== bench: -> %ux%u, %d frames ==\n", W, H, frames);
        for (int i = 0; i < frames; ++i) {
            chart.tick(0.016);
            mount.paint_all();
        }
        std::fprintf(stderr, "== bench done (animating=%d) ==\n",
                     static_cast<int>(chart.animating()));
        return 0;
    }

    const bool smoke = (argc > 1 && std::string(argv[1]) == "--smoke");
    bool profile = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--profile") profile = true;
    }
    // 并排：左 combo(600) + 右 HSI(600)；窗口 1200 宽。
    const std::uint32_t W = 1200;
    const std::uint32_t H = 400;

    termchart_gui_win32::Win32Backend backend(W, H,
                                              "termchart_gui - combo + HSI K-line (hover / resize)");
    if (!backend.valid()) {
        std::fprintf(stderr, "Win32Backend: 无法创建窗口（需要交互桌面会话）\n");
        return 2;
    }

    Container root("root");
    root.set_bounds(Rect{0, 0, static_cast<double>(W), static_cast<double>(H)});
    auto fl = std::make_unique<FlexLayout>();
    fl->direction = FlexDirection::Row;  // 横向并排：两图各占一半
    fl->cross_align = FlexAlign::Stretch;
    fl->gap = 0.0;
    root.set_layout(std::move(fl));

    // 左：combo 组合图（默认插头 ComboRenderer）。
    auto& combo = root.emplace_child<ChartWidget>("combo");
    combo.set_renderer(std::make_shared<ComboRenderer>());
    combo.set_data(kLabels, kBars, kLine);
    combo.set_bounds(Rect{0, 0, 600, H});  // 初始提示；Row 布局每帧重排为等分

    // 右：把 HsiKLineRenderer 插头插进同一个"万能插座"。
    auto& hsi = root.emplace_child<ChartWidget>("hsi");
    auto hsi_renderer = std::make_shared<HsiKLineRenderer>();
    hsi_renderer->set_data(kHsiLabels, kHsiCandles);
    hsi.set_renderer(hsi_renderer);
    hsi.set_bounds(Rect{600, 0, 600, H});  // 初始提示；Row 布局每帧重排为等分

    Mount mount(root, backend);
    mount.paint_all();

    // resize 拖动中实时调柱宽/间距：在后端注册同步重渲染钩子。
    backend.set_render_hook([&](std::uint32_t rw, std::uint32_t rh) {
        combo.set_dragging(true);
        hsi.set_dragging(true);
        root.set_bounds(Rect{0, 0, static_cast<double>(rw), static_cast<double>(rh)});
        combo.request_snap();
        hsi.request_snap();
        mount.paint_all();
    });

    auto handle = [&](const Event& e, bool& running) -> bool {
        switch (e.type) {
            case EventType::Quit:
                running = false;
                return false;
            case EventType::Resize:
                // 松手：退出拖动态，按最终宽度重算抽稀（增删数据显示）。
                combo.set_dragging(false);
                hsi.set_dragging(false);
                root.set_bounds(Rect{0, 0, static_cast<double>(e.width),
                                     static_cast<double>(e.height)});
                return true;
            default:
                return mount.dispatch_event(e);
        }
    };

    if (smoke) {
        bool running = true;
        for (int i = 0; i < 5 && running; ++i) {
            Event e;
            while (backend.poll_event(e)) (void)handle(e, running);
            mount.paint_all();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        backend.request_close();
        for (int i = 0; i < 100; ++i) {
            bool r = true;
            Event e;
            while (backend.poll_event(e)) (void)handle(e, r);
            if (!r) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::fprintf(stderr, "smoke: combo + HSI rendered, window closed cleanly\n");
        return 0;
    }

    auto last = std::chrono::steady_clock::now();
    bool running = true;
    while (running) {
        bool dirty = false;
        Event e;
        while (backend.poll_event(e)) {  // 抽干积压消息（拖拽时 WM_SIZE 不爆炸）
            if (handle(e, running)) dirty = true;
        }
        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - last).count();
        last = now;
        if (combo.animating() || hsi.animating()) {
            combo.tick(dt);  // 真实 dt：指数趋近帧率解耦
            hsi.tick(dt);
            dirty = true;
        }
        if (dirty) {
            const auto p0 = std::chrono::steady_clock::now();
            mount.paint_all();
            const double pms = std::chrono::duration<double, std::milli>(
                                   std::chrono::steady_clock::now() - p0)
                                   .count();
            if (profile && pms > 33.0) {
                std::fprintf(stderr, "warn: paint_all %.1f ms (dropped frame)\n", pms);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(8));  // 空闲才睡
        }
    }
    return 0;
}
