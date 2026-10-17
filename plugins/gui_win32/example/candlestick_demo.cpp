// candlestick_demo.cpp —— 恒生指数日 K 线演示（Win32 原生窗口）
//
// 数据：恒生指数(^HSI) 日线 OHLC，最近 25 个交易日（2026-09-11 ~ 2026-10-16），
// 来自东方财富历史行情接口（push2his.eastmoney.com, secid=100.HSI, klt=101）。
// 悬停某根蜡烛看 OHLC 信息框；拖边框窗口自适应；ESC / 关闭退出；--smoke 无交互自关闭。
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "termchart_gui/backend_tk/tk_canvas.hpp"
#include "termchart_gui/backend_tk/tk_window.hpp"
#include "termchart_gui/fwd.hpp"
#include "termchart_gui/widget_tree/candlestick_widget.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/mount.hpp"
#include "termchart_gui_win32/win32_backend.hpp"

using termchart_gui::CandlestickWidget;
using termchart_gui::Container;
using termchart_gui::Event;
using termchart_gui::EventType;
using termchart_gui::Mount;
using termchart_gui::Rect;

int main(int argc, char** argv) {
    const bool smoke = (argc > 1 && std::string(argv[1]) == "--smoke");
    const std::uint32_t W = 760;
    const std::uint32_t H = 460;

    // 恒生指数日线（date, open, high, low, close），2026-09-11 ~ 2026-10-16。
    const std::vector<std::string> kLabels = {
        "2026-09-11", "2026-09-14", "2026-09-15", "2026-09-16", "2026-09-17",
        "2026-09-18", "2026-09-21", "2026-09-22", "2026-09-23", "2026-09-24",
        "2026-09-25", "2026-09-28", "2026-09-29", "2026-09-30", "2026-10-02",
        "2026-10-05", "2026-10-06", "2026-10-07", "2026-10-08", "2026-10-09",
        "2026-10-12", "2026-10-13", "2026-10-14", "2026-10-15", "2026-10-16"};
    const std::vector<CandlestickWidget::Candle> kCandles = {
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

    termchart_gui_win32::Win32Backend backend(W, H,
                                              "termchart_gui - HSI candlestick (hover / resize)");
    if (!backend.valid()) {
        return 2;
    }

    Container root("root");
    root.set_bounds(Rect{0, 0, static_cast<double>(W), static_cast<double>(H)});
    auto& chart = root.emplace_child<CandlestickWidget>("chart");
    chart.set_data(kLabels, kCandles);

    Mount mount(root, backend);
    mount.paint_all();

    auto handle = [&](const Event& e, bool& running) -> bool {
        switch (e.type) {
            case EventType::Quit:
                running = false;
                return false;
            case EventType::Resize:
                root.set_bounds(Rect{0, 0, static_cast<double>(e.width),
                                     static_cast<double>(e.height)});
                return true;
            default:
                return mount.dispatch_event(e);
        }
    };

    if (smoke) {
        bool running = true;
        for (int i = 0; i < 4 && running; ++i) {
            Event e;
            while (backend.poll_event(e)) (void)handle(e, running);
            mount.paint_all();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        backend.request_close();
        std::fprintf(stderr, "smoke: HSI candlestick rendered, window closed cleanly\n");
        return 0;
    }

    bool running = true;
    while (running) {
        bool dirty = false;
        Event e;
        while (backend.poll_event(e)) {
            if (handle(e, running)) dirty = true;
        }
        if (dirty) mount.paint_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    return 0;
}
