// progress_ring.cpp —— 进度环实现：椭圆采样弧 + 中心百分比
#include "termchart/chart/indicator/progress_ring.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "termchart/core/rect.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::chart {
namespace {

constexpr char32_t kArc = U'\u25CF';    // 已完成点
constexpr char32_t kTrack = U'\u00B7';  // 轨道点

Rect content_area(const DrawContext& ctx, const Chart& chart) {
    const double w = static_cast<double>(ctx.canvas.width());
    const double h = static_cast<double>(ctx.canvas.height());
    Rect area{chart.margin_left(), chart.margin_top(),
              std::max(0.0, w - chart.margin_left() - chart.margin_right()),
              std::max(0.0, h - chart.margin_top() - chart.margin_bottom())};
    if (ctx.draw_title && !chart.title().empty() && area.h >= 2.0) {
        layout::draw_title(ctx.canvas, chart.title(), area);
        area = Rect{area.x, area.y + 1.0, area.w, area.h - 1.0};
    }
    return area;
}

}  // namespace

void ProgressRing::draw(DrawContext ctx) {
    double fraction = std::isfinite(fraction_) ? fraction_ : 0.0;
    fraction = std::min(1.0, std::max(0.0, fraction));

    const Rect area = content_area(ctx, *this);
    const double rx = (area.w - 2.0) / 2.0;
    const double ry = (area.h - 2.0) / 2.0;
    const double cx = area.x + area.w / 2.0;
    const double cy = area.y + area.h / 2.0;

    if (rx >= 1.0 && ry >= 1.0) {
        // 椭圆采样：从顶部起、屏幕坐标下顺时针；步数与周长成正比，保证相邻
        // 采样点不留洞。i/N < fraction 判定该点是否已被扫过。
        const long long steps = std::max(64LL, std::llround((rx + ry) * 8.0));
        const Style arc_st = Style{}.set_fg(color_);
        const Style track_st = Style{}.set_fg(options_.track_color);
        for (long long i = 0; i < steps; ++i) {
            const double theta =
                -1.5707963267948966 +
                6.283185307179586 * static_cast<double>(i) / static_cast<double>(steps);
            const double px = cx + rx * std::cos(theta);
            const double py = cy + ry * std::sin(theta);
            const auto col = std::llround(px);
            const auto row = std::llround(py);
            if (col < 0 || row < 0 ||
                col >= static_cast<long long>(ctx.canvas.width()) ||
                row >= static_cast<long long>(ctx.canvas.height())) {
                continue;
            }
            const bool done =
                fraction >= 1.0 ||
                static_cast<double>(i) / static_cast<double>(steps) < fraction;
            ctx.canvas.put(static_cast<std::uint32_t>(col),
                           static_cast<std::uint32_t>(row), done ? kArc : kTrack,
                           done ? arc_st : track_st);
        }
    }

    // 中心百分比（环太小也要给读数）。
    if (options_.show_percent) {
        char buf[16];
        const int pct = static_cast<int>(fraction * 100.0 + 0.5);
        std::snprintf(buf, sizeof(buf), "%d%%", pct);
        const int text_w = util::display_width(buf);
        const double x = cx - static_cast<double>(text_w) / 2.0;
        ctx.canvas.draw_text(Point{x < 0.0 ? 0.0 : x, cy}, buf, Style{});
    }
}

}  // namespace termchart::chart
