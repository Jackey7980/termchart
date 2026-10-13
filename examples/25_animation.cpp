// 25_animation.cpp —— 动画与缓动：注入时钟扫描帧序列（输出逐字节可复现）
//
// 消费 app/animation.hpp：Animation（start_at 注入起点）+ ease() 缓动映射。
// 全程走纯函数路径 value_at()，不读真实时钟——同一份输出在任何机器上一致，
// headless / CI 可直接快照。进度条的落点就是缓动曲线的形状。
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/app/animation.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

constexpr double kDuration = 1.0;  // 秒（时间单位与 util/timer.hpp 一致）
constexpr int kSteps = 6;          // 每条曲线扫 kSteps+1 帧（含 t=0 与 t=1）
constexpr double kTrack = 44.0;    // 轨道格宽

void draw_frame(canvas::Canvas& c, const char* name, int frame, double t, double v) {
    char head[96] = {};
    std::snprintf(head, sizeof(head), "%s  frame %d/%d  t=%.3f  v=%.3f", name, frame, kSteps, t,
                  v);
    c.draw_text(Point{0.0, 0.0}, head, Style{}.set_fg(Color::ansi16(7)));
    c.draw_line(Point{0.0, 2.0}, Point{kTrack, 2.0}, U'\u2500', Style{}.set_fg(Color::ansi16(8)));
    const double x = v * kTrack;
    if (x >= 1.0) {
        c.fill_rect(Rect{0.0, 2.0, x, 1.0}, U'\u2588', Style{}.set_fg(Color::ansi16(6)));
    }
    c.put(Point{x, 2.0}, U'\u25CF', Style{}.set_fg(Color::ansi16(3)));
}

}  // namespace

int main() {
    const std::vector<std::pair<const char*, app::Easing>> easings{
        {"Linear", app::Easing::Linear},
        {"EaseOutQuad", app::Easing::EaseOutQuad},
        {"EaseInOutCubic", app::Easing::EaseInOutCubic},
    };

    render::Renderer renderer;
    bool ok = true;
    for (const auto& easing : easings) {
        app::Animation anim(kDuration, easing.second);
        anim.start_at(0.0);  // 注入起点；此后 value_at 是纯函数，不读时钟
        if (api::write_stdout(std::string("\n== easing: ") + easing.first + " ==\n") !=
            ErrorCode::Ok) {
            ok = false;
        }
        for (int f = 0; f <= kSteps; ++f) {
            const double t = kDuration * static_cast<double>(f) / static_cast<double>(kSteps);
            const double v = anim.value_at(t);
            canvas::Canvas c(46, 4);
            draw_frame(c, easing.first, f, t, v);
            if (api::write_stdout(renderer.render_full(c.to_buffer())) != ErrorCode::Ok) {
                ok = false;
            }
            if (api::write_stdout("\n") != ErrorCode::Ok) {
                ok = false;
            }
        }
    }
    api::flush_stdout();
    return ok ? 0 : 1;
}
