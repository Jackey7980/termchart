// 26_realtime_stream.cpp —— 实时数据流：主循环驱动滚动窗口折线（注入时钟）
//
// 消费 app/loop.hpp：Loop 的时钟可注入——本示例注入"脚本时钟"（每次读 +0.25s），
// 帧时刻完全确定，不依赖真实时间流逝，headless/CI 里 run(N) 跑完即退。
// 每帧：采样一个确定性信号 → 滚动窗口（定长、先进先出）→ 重画整幅折线。
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/app/loop.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

constexpr std::size_t kWindow = 48;  // 滚动窗口长度
constexpr double kYMin = -1.4;
constexpr double kYMax = 1.4;

}  // namespace

int main() {
    render::Renderer renderer;
    std::deque<double> window;
    std::uint64_t frame_no = 0;
    double now = 0.0;
    bool ok = true;

    // 注入脚本时钟：每读一次 +0.25s（真实程序这里是 steady_clock，见 loop.hpp 头注）。
    app::Loop loop([&now] { now += 0.25; return now; });

    (void)loop.add_tick([&](double delta) {
        ++frame_no;
        const double t = now;
        window.push_back(std::sin(0.9 * t) + 0.3 * std::sin(2.7 * t));
        while (window.size() > kWindow) {
            window.pop_front();
        }

        canvas::Canvas c(64, 14);
        char head[96] = {};
        std::snprintf(head, sizeof(head),
                      "realtime stream  frame %llu  t=%.2fs  delta=%.2fs  window=%zu",
                      static_cast<unsigned long long>(frame_no), t, delta, window.size());
        c.draw_text(Point{0.0, 0.0}, head, Style{}.set_fg(Color::ansi16(7)));

        const Rect area{1.0, 2.0, 61.0, 9.0};
        const Style axis = Style{}.set_fg(Color::ansi16(8));
        c.draw_line(Point{area.x - 1.0, area.y - 1.0}, Point{area.right(), area.y - 1.0},
                    U'\u2500', axis);
        c.draw_line(Point{area.x - 1.0, area.bottom() + 1.0},
                    Point{area.right(), area.bottom() + 1.0}, U'\u2500', axis);
        c.draw_line(Point{area.x - 1.0, area.y - 1.0},
                    Point{area.x - 1.0, area.bottom() + 1.0}, U'\u2502', axis);
        c.draw_line(Point{area.right(), area.y - 1.0},
                    Point{area.right(), area.bottom() + 1.0}, U'\u2502', axis);

        // 滚动窗口折线：横轴 = 窗口内序号（满窗后整体左移），纵轴固定域。
        const Style data = Style{}.set_fg(Color::ansi16(2));
        const std::size_t n = window.size();
        const auto to_screen = [&](std::size_t i) {
            const double nx = static_cast<double>(i) / static_cast<double>(kWindow - 1);
            const double ny = (window[i] - kYMin) / (kYMax - kYMin);
            return Point{area.x + nx * (area.w - 1.0),
                         area.y + (1.0 - ny) * (area.h - 1.0)};
        };
        for (std::size_t i = 0; i + 1 < n; ++i) {
            c.draw_line(to_screen(i), to_screen(i + 1), U'\u2022', data);
        }
        if (n > 0) {
            c.put(to_screen(n - 1), U'\u25CF', Style{}.set_fg(Color::ansi16(3)));  // 最新样本
        }

        if (api::write_stdout("\n== frame " + std::to_string(frame_no) +
                              " (scripted clock) ==\n") != ErrorCode::Ok) {
            ok = false;
        }
        if (api::write_stdout(renderer.render_full(c.to_buffer())) != ErrorCode::Ok) {
            ok = false;
        }
    });

    const std::uint64_t frames = loop.run(8);
    char tail[96] = {};
    std::snprintf(tail, sizeof(tail),
                  "\n(stream ended: %llu frames, window=%zu; scripted clock => deterministic)\n",
                  static_cast<unsigned long long>(frames), window.size());
    if (api::write_stdout(tail) != ErrorCode::Ok) {
        ok = false;
    }
    api::flush_stdout();
    return ok ? 0 : 1;
}
