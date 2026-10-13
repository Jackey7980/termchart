// 41_reactive_dashboard.cpp —— 响应式仪表盘：Observable 值变更驱动面板刷新
//
// 消费 widget/reactive.hpp：Observable<T> + RAII Subscription。
// 事件流："set 新值 → 订阅者收到通知 → 重画面板"。值未变不通知（脚本里专门
// 演示）；Subscription 析构即退订，无手工簿记。
#include <cstdio>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/sparkline.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/widget/reactive.hpp"
#include "termchart/widget/theme_dark.hpp"
#include "termchart/widget/widget.hpp"

namespace {

using namespace termchart;

void draw_panel(canvas::Canvas& c, const widget::ThemePalette& theme, int refresh, double value,
                const std::vector<double>& history) {
    const Style fg = widget::style_fg(theme.foreground);
    const Style dim = widget::style_fg(theme.text_dim);
    const Style border = widget::style_fg(theme.border);
    const Style accent = widget::style_fg(theme.accent);

    const double w = static_cast<double>(c.width());
    const double h = static_cast<double>(c.height());
    c.draw_line(Point{0.0, 1.0}, Point{w - 1.0, 1.0}, U'\u2500', border);
    c.draw_line(Point{0.0, h - 1.0}, Point{w - 1.0, h - 1.0}, U'\u2500', border);
    c.draw_text(Point{2.0, 0.0}, "temperature sensor", fg);

    char big[64] = {};
    std::snprintf(big, sizeof(big), "%.1f C", value);
    c.draw_text(Point{2.0, 3.0}, big, accent);

    char meta[64] = {};
    std::snprintf(meta, sizeof(meta), "refresh #%d  history=%zu", refresh, history.size());
    c.draw_text(Point{2.0, 5.0}, meta, dim);

    // 历史趋势：sparkline 无轴无网格，正好当面板内的小趋势（子画布 blit 进面板）。
    chart::Sparkline spark;
    spark.set_values(history);
    spark.set_color(Color::rgb(theme.accent));
    canvas::Canvas trend(static_cast<std::uint32_t>(w - 4.0), 5);
    (void)spark.draw(trend);
    for (std::uint32_t y = 0; y < trend.height(); ++y) {
        for (std::uint32_t x = 0; x < trend.width(); ++x) {
            const auto& cell = trend.at(x, y);
            if (cell.cp != U' ') {
                c.put(2 + x, 7 + y, cell.cp, cell.style);
            }
        }
    }
}

}  // namespace

int main() {
    const widget::ThemePalette theme = widget::kDarkTheme;
    render::Renderer renderer;
    bool ok = true;

    widget::Observable<double> temperature{20.0};
    std::vector<double> history{20.0};
    int refresh = 0;

    // RAII 订阅：回调里只做"面板刷新"这一件事；sub 析构即退订。
    const widget::Subscription sub = temperature.subscribe(
        [&](const double& new_value, const double& old_value) {
            ++refresh;
            history.push_back(new_value);
            canvas::Canvas c(40, 13);
            draw_panel(c, theme, refresh, new_value, history);
            char head[64] = {};
            std::snprintf(head, sizeof(head), "\n== refresh #%d: %.1f -> %.1f ==\n", refresh,
                          old_value, new_value);
            if (api::write_stdout(head) != ErrorCode::Ok) {
                ok = false;
            }
            if (api::write_stdout(renderer.render_full(c.to_buffer())) != ErrorCode::Ok) {
                ok = false;
            }
        });

    temperature.set(22.5);  // 变更 → 通知 → 重画
    temperature.set(22.5);  // 值未变 → 不通知（无输出）
    temperature.set(19.0);  // 变更 → 通知
    temperature.set(25.0);  // 变更 → 通知

    // 收尾帧：汇总通知次数（应 = 3，同值 set 不计数）。
    canvas::Canvas summary(40, 13);
    draw_panel(summary, theme, refresh, temperature.get(), history);
    if (api::write_stdout("\n== summary (notifications only on change) ==\n") != ErrorCode::Ok) {
        ok = false;
    }
    if (api::write_stdout(renderer.render_full(summary.to_buffer())) != ErrorCode::Ok) {
        ok = false;
    }

    api::flush_stdout();
    return ok ? 0 : 1;  // sub 在此析构 → 退订（RAII）
}
