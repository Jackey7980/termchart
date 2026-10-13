// 24_theme_switching.cpp —— 运行时切主题：同一图表用两张主题色板各渲一帧
//
// 消费 widget/theme_dark|light.hpp（ThemePalette 值语义色板）：
// 绘制函数只认色板槽位（foreground/border/accent/text_dim/success/danger），
// 不认具体主题——切主题 = 换一张表把同一份绘制代码重跑一遍。
// 无全局状态、无注册：色板是 inline constexpr 值，按引用传入即可。
#include <cmath>
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/widget/theme_dark.hpp"
#include "termchart/widget/theme_light.hpp"
#include "termchart/widget/widget.hpp"

namespace {

using namespace termchart;

constexpr double kXMax = 60.0;
constexpr double kYMin = -1.2;
constexpr double kYMax = 1.2;

void draw_dashboard(canvas::Canvas& c, const widget::ThemePalette& theme) {
    const Style fg = widget::style_fg(theme.foreground);
    const Style dim = widget::style_fg(theme.text_dim);
    const Style border = widget::style_fg(theme.border);
    const Style accent = widget::style_fg(theme.accent);
    const Style pass = widget::style_fg(theme.success);
    const Style limit = widget::style_fg(theme.danger);

    const double w = static_cast<double>(c.width());
    const double h = static_cast<double>(c.height());
    c.draw_line(Point{0.0, 1.0}, Point{w - 1.0, 1.0}, U'\u2500', border);
    c.draw_line(Point{0.0, h - 1.0}, Point{w - 1.0, h - 1.0}, U'\u2500', border);
    c.draw_text(Point{2.0, 0.0}, "service dashboard", fg);

    const Rect area{2.0, 3.0, w - 5.0, h - 7.0};
    const auto to_screen = [&](Point data) {
        const double nx = data.x / kXMax;
        const double ny = (data.y - kYMin) / (kYMax - kYMin);
        return Point{area.x + nx * (area.w - 1.0), area.y + (1.0 - ny) * (area.h - 1.0)};
    };

    // SLO 阈值线（danger 槽位）。
    c.draw_line(to_screen(Point{0.0, 0.6}), to_screen(Point{kXMax, 0.6}), U'\u2500', limit);

    // 数据折线（accent 槽位）：确定性双正弦。
    for (int i = 0; i < 120; ++i) {
        const double x0 = static_cast<double>(i) / 2.0;
        const double x1 = static_cast<double>(i + 1) / 2.0;
        const double y0 = 0.55 * std::sin(1.7 * x0) + 0.30 * std::sin(5.1 * x0 + 1.0);
        const double y1 = 0.55 * std::sin(1.7 * x1) + 0.30 * std::sin(5.1 * x1 + 1.0);
        c.draw_line(to_screen(Point{x0, y0}), to_screen(Point{x1, y1}), U'\u2022', accent);
    }

    c.draw_text(Point{2.0, h - 3.0}, "latency p99", dim);
    c.draw_text(Point{16.0, h - 3.0}, "[ok] within SLO", pass);
    c.draw_text(Point{2.0, h - 2.0}, "same draw code, ThemePalette swapped at runtime", dim);
}

}  // namespace

int main() {
    render::Renderer renderer;
    bool ok = true;
    const auto emit = [&](const char* name, const widget::ThemePalette& theme) {
        canvas::Canvas c(64, 18);
        draw_dashboard(c, theme);
        if (api::write_stdout(std::string("\n== theme: ") + name + " ==\n") != ErrorCode::Ok) {
            ok = false;
        }
        if (api::write_stdout(renderer.render_full(c.to_buffer())) != ErrorCode::Ok) {
            ok = false;
        }
    };

    emit("dark", widget::kDarkTheme);
    emit("light", widget::kLightTheme);
    if (api::write_stdout("\n(theme switching = value-semantic palette swap; "
                          "widget/theme_mono.hpp is the monochrome fallback)\n") !=
        ErrorCode::Ok) {
        ok = false;
    }
    api::flush_stdout();
    return ok ? 0 : 1;
}
