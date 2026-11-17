// candlestick_demo.cpp —— termchart_gui 英雄 Demo：恒生指数日 K 线（蜡烛图）
//
// 用「骨肉分离」架构：widget_tree 负责 Widget 树/布局/交互/事件派发，HsiKLineRenderer
// 负责 K 线绘制并插进 ChartWidget 的「万能插座」（动画/缓存/降分辨率/淡入/悬停十字轴）。
// 本 Demo 演示：
//   · 悬停十字轴 + 信息框（OHLC 实时读数）
//   · 鼠标滚轮缩放 / 拖拽平移（ChartWidget 自动接管，Pan/Zoom 数据探索）
//   · 底部成交量副图（与 K 线联动同一视图窗口）
//   · 'T' 键一键切换主题（DefaultDark / GitHubDark / AppleLight）
// 同一份 widget_tree 也可挂在 Qt/Tk/X11/Cocoa/JNI 上（宏切换后端）。
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/gui.hpp"
#include "termchart_gui/layout/flex.hpp"
#include "termchart_gui/theme.hpp"
#include "termchart_gui/widget_tree/chart_widget.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/hsi_kline_renderer.hpp"
#include "termchart_gui/widget_tree/label.hpp"
#include "termchart_gui/widget_tree/mount.hpp"

#ifdef TERMCHART_GUI_WIN32
#include "termchart_gui_win32/win32_backend.hpp"
#elif defined(TERMCHART_GUI_QT)
#include "termchart_gui_qt/qt_backend.hpp"
#endif

using namespace termchart_gui;

// 简易高斯噪声（Box-Muller），演示用。
static double gauss(double sigma) {
    static std::mt19937 rng(20260911);
    double u = 1.0 - (static_cast<double>(rng()) + 0.5) / (rng.max() + 1.0);
    double v = (static_cast<double>(rng()) + 0.5) / (rng.max() + 1.0);
    return sigma * std::sqrt(-2.0 * std::log(u)) * std::cos(2.0 * 3.141592653589793 * v);
}

int main(int argc, char** argv) {
    const bool smoke = (argc > 1) && std::string(argv[1]) == "--smoke";
    if (argc > 1 && std::string(argv[1]) == "--help") {
        std::printf("candlestick_demo [--smoke]\n  --smoke  无窗口渲染数帧后退出（CI 用）\n");
        return 0;
    }
    // 英雄 Demo 默认走 GitHub 深色主题（一键 'T' 可切 DefaultDark / AppleLight）。
    set_theme(GitHubDark());

    const int N = 25;
    std::vector<std::string> labels;
    std::vector<HsiKLineRenderer::Candle> candles;
    std::vector<double> vols;
    double close = 25200.0;
    for (int i = 0; i < N; ++i) {
        const double open = close;
        const double drift = gauss(90.0) + (i > 0 ? (candles.back().close - open) * 0.15 : 0.0);
        const double c = open + drift;
        const double high = std::max(open, c) + std::fabs(gauss(45.0)) + 20.0;
        const double low = std::min(open, c) - std::fabs(gauss(45.0)) - 20.0;
        candles.push_back({open, high, low, c});
        char buf[16];
        std::snprintf(buf, sizeof(buf), "09-%02d", 1 + (i % 28));
        labels.push_back(buf);
        // 成交量（演示合成；真实场景接 tick 量）
        const double range = (high - low);
        vols.push_back((0.8 + 0.4 * std::fabs(std::sin(i * 0.7))) * (600000.0 + range * 4000.0));
        close = c;
    }

    const std::uint32_t W = 900, H = 500;

#ifdef TERMCHART_GUI_WIN32
    Win32Backend backend(W, H, "termchart — HSI K-Line");
#elif defined(TERMCHART_GUI_QT)
    QtBackend backend(W, H, "termchart — HSI K-Line");
#else
    (void)W;
    (void)H;
    HeadlessBackend backend(900, 500);
#endif

    // 无显示环境（CI 无桌面会话）：smoke 时改走无头后端验证渲染器不崩，否则报错退出。
    // HeadlessBackend（无宏分支）无 valid()，故仅真实窗口后端才检查。
    Backend* bp = &backend;
    std::unique_ptr<HeadlessBackend> hb;
#if defined(TERMCHART_GUI_WIN32) || defined(TERMCHART_GUI_QT)
    if (!backend.valid()) {
        if (smoke) {
            hb = std::make_unique<HeadlessBackend>(W, H);
            bp = hb.get();
        } else {
            std::fprintf(stderr, "GUI backend: 无法创建窗口（需要交互桌面会话）\n");
            return 2;
        }
    }
#endif

    Container root("root");
    root.set_bounds(Rect{0.0, 0.0, static_cast<double>(W), static_cast<double>(H)});
    auto fl = std::make_unique<FlexLayout>();
    fl->direction = FlexDirection::Column;  // 垂直：标题 + 图表 + 底注
    fl->cross_align = FlexAlign::Stretch;
    fl->gap = 0.0;
    root.set_layout(std::move(fl));

    const GuiTheme th0 = current_theme();
    auto& title = root.emplace_child<Label>(
        "title", "HSI Daily — Pan/Zoom: wheel & drag · 'T' theme",
        th0.foreground, th0.background);
    title.set_bounds(Rect{0.0, 0.0, static_cast<double>(W), 22.0});
    title.set_hpolicy(SizePolicy::Fixed);
    title.set_vpolicy(SizePolicy::Fixed);

    // 把 HsiKLineRenderer（带成交量副图）插头插进 ChartWidget「万能插座」。
    auto& chart = root.emplace_child<ChartWidget>("chart");
    auto hsi = std::make_shared<HsiKLineRenderer>();
    hsi->set_data(labels, candles);
    hsi->set_volumes(vols);
    chart.set_renderer(hsi);

    auto& demo = root.emplace_child<Label>(
        "demo", "termchart_gui · K-line hero demo", th0.text_dim, th0.background);
    demo.set_bounds(Rect{0.0, 0.0, static_cast<double>(W), 18.0});
    demo.set_hpolicy(SizePolicy::Fixed);
    demo.set_vpolicy(SizePolicy::Fixed);

    Mount mount(root, *bp);
    mount.paint_all();

    if (smoke) {
        for (int i = 0; i < 5; ++i) {
            Event e;
            while (bp->poll_event(e)) (void)mount.dispatch_event(e);
            chart.tick(0.016);
            mount.paint_all();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        std::fprintf(stderr, "smoke: HSI K-line rendered, no crash\n");
        return 0;
    }

    auto last = std::chrono::steady_clock::now();
    bool running = true;
    while (running) {
        bool dirty = false;
        Event e;
        while (bp->poll_event(e)) {
            if (e.type == EventType::Quit) {
                running = false;
                break;
            }
            if (e.type == EventType::KeyPress && (e.key == 't' || e.key == 'T')) {
                cycle_theme();  // 一键换肤：渲染器与控件都读 current_theme()
                const GuiTheme th = current_theme();
                title.set_fg(th.foreground);
                title.set_bg(th.background);
                demo.set_fg(th.text_dim);
                demo.set_bg(th.background);
                dirty = true;
                continue;
            }
            if (e.type == EventType::Resize) {
                // 松手：按最终尺寸重排（视图窗口不变，仅缩放像素）。
                chart.set_dragging(false);
                root.set_bounds(Rect{0.0, 0.0, static_cast<double>(e.width),
                                     static_cast<double>(e.height)});
                chart.request_snap();
                dirty = true;
                continue;
            }
            if (mount.dispatch_event(e)) {
                dirty = true;  // 悬停/缩放/平移等交互置脏
            }
        }
        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - last).count();
        last = now;
        if (chart.animating()) {  // 真实 dt：缩放/平移几何指数趋近，帧率解耦
            chart.tick(dt);
            dirty = true;
        }
        if (dirty) {
            mount.paint_all();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(8));  // 空闲才睡
        }
    }
    return 0;
}
