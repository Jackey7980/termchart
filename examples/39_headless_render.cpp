// 39_headless_render.cpp —— CI 无终端渲染：全程不碰 stdout，收尾一次性输出
//
// 消费 app/headless.hpp：render_once（一次性）与 HeadlessSession（多帧增量）。
// 与实时路径共享同一个 render::Renderer，字节流与真终端一致；差异只在"谁写
// stdout"——这里攒够字节串最后一次性写出（CI 里可重定向、可快照）。
// 不初始化终端、不进原始模式、不触碰 term/。
#include <cmath>
#include <cstdio>
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/app/headless.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"

namespace {

using namespace termchart;

// 画一帧正弦波（phase 不同 → 帧间有差异，供增量渲染用）。
void draw_wave(canvas::Canvas& c, double phase, const char* tag) {
    c.draw_text(Point{0.0, 0.0}, std::string("headless render: ") + tag,
                Style{}.set_fg(Color::ansi16(7)));
    const Rect area{1.0, 2.0, static_cast<double>(c.width()) - 3.0,
                    static_cast<double>(c.height()) - 4.0};
    const Style data = Style{}.set_fg(Color::ansi16(2));
    const auto map = [&](double x, double y) {
        const double nx = x / 60.0;
        const double ny = (y + 1.0) / 2.0;  // y ∈ [-1, 1] → [0, 1]
        return Point{area.x + nx * (area.w - 1.0), area.y + (1.0 - ny) * (area.h - 1.0)};
    };
    for (int i = 0; i < 120; ++i) {
        const double x0 = static_cast<double>(i) / 2.0;
        const double x1 = static_cast<double>(i + 1) / 2.0;
        const double y0 = 0.8 * std::sin(0.5 * x0 + phase);
        const double y1 = 0.8 * std::sin(0.5 * x1 + phase);
        c.draw_line(map(x0, y0), map(x1, y1), U'\u2022', data);
    }
}

}  // namespace

int main() {
    bool ok = true;
    std::string report;

    // 一次性渲染：render_once 不初始化终端、不写 stdout，字节串直接交给调用方。
    const Result<std::string> once =
        app::render_once(56, 14, [](canvas::Canvas& c) { draw_wave(c, 0.0, "render_once"); });
    if (!once.has_value()) {
        (void)api::write_stderr("render_once failed\n");
        return 1;
    }
    report += "\n== frame 1: render_once ==\n";
    report += once.value();

    // 多帧会话：HeadlessSession 保留 Renderer 的 prev 状态，frame() 出增量帧。
    app::HeadlessSession session(56, 14);
    draw_wave(session.canvas(), 0.0, "session frame 1 (full)");
    const std::string f1 = session.frame();

    draw_wave(session.canvas(), 0.7, "session frame 2 (increment)");
    const std::string f2 = session.frame();

    const std::string f3 = session.frame();  // 画面未变 → 空串（增量路径契约）

    char sizes[128] = {};
    std::snprintf(sizes, sizeof(sizes),
                  "\n[frame sizes] full=%zu  delta=%zu  unchanged=%zu (empty as contracted)\n",
                  f1.size(), f2.size(), f3.size());
    report += sizes;
    report += "\n== frame 2 bytes (delta against frame 1) ==\n";
    report += f2;

    if (api::write_stdout(report) != ErrorCode::Ok) {
        ok = false;
    }
    api::flush_stdout();
    return ok ? 0 : 1;
}
