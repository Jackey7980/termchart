// thermometer.cpp —— 温度计实现：竖管液柱 + 球泡 + 刻度
#include "termchart/chart/indicator/thermometer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

#include "termchart/core/rect.hpp"
#include "termchart/layout/title.hpp"

namespace termchart::chart {
namespace {

constexpr char32_t kFill = U'\u2588';  // █
constexpr char32_t kWall = U'\u2502';  // │
constexpr char32_t kTick = U'\u2500';  // ─

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

// 液色随填充比例分档：青 → 绿 → 黄 → 红。
Color level_color(double fraction01) noexcept {
    if (fraction01 < 0.25) {
        return default_color(DefaultColor::Cyan);
    }
    if (fraction01 < 0.5) {
        return default_color(DefaultColor::Accent);
    }
    if (fraction01 < 0.75) {
        return default_color(DefaultColor::Amber);
    }
    return default_color(DefaultColor::Warn);
}

}  // namespace

void Thermometer::draw(DrawContext ctx) {
    const Rect area = content_area(ctx, *this);
    // 最小可用：1 格管 + 2 格球泡。
    if (area.w < 3.0 || area.h < 3.0) {
        return;
    }

    double lo = std::isfinite(options_.vmin) ? options_.vmin : 0.0;
    double hi = std::isfinite(options_.vmax) ? options_.vmax : 0.0;
    if (hi < lo) {
        std::swap(lo, hi);  // 域颠倒按交换处理，不炸
    }

    const long long tube_h = std::max(1LL, std::llround(area.h) - 2);  // 底行留给球泡
    const long long tx = std::llround(area.x + (area.w - 1.0) / 2.0);
    const long long top = std::llround(area.y);
    const long long bulb_row = top + tube_h + 1;
    const long long bottom = top + tube_h;  // 管内最下一行
    const long long canvas_w = static_cast<long long>(ctx.canvas.width());
    const long long canvas_h = static_cast<long long>(ctx.canvas.height());

    // 管壁（左右各 1 格）+ 球泡（底行 3 格宽 █）。
    const Style wall_st{};
    for (long long x = tx - 1; x <= tx + 1; x += 2) {
        if (x >= 0 && x < canvas_w) {
            for (long long y = top; y <= bottom && y < canvas_h; ++y) {
                ctx.canvas.put(static_cast<std::uint32_t>(x),
                               static_cast<std::uint32_t>(y), kWall, wall_st);
            }
        }
    }
    const Style bulb_st = Style{}.set_fg(level_color(1.0));
    for (long long x = tx - 1; x <= tx + 1; ++x) {
        if (x >= 0 && x < canvas_w && bulb_row >= 0 && bulb_row < canvas_h) {
            ctx.canvas.put(static_cast<std::uint32_t>(x),
                           static_cast<std::uint32_t>(bulb_row), kFill, bulb_st);
        }
    }

    // 液柱：底部向上填 fill_rows 格。
    double fraction = hi > lo ? (value_ - lo) / (hi - lo) : 0.0;
    if (!std::isfinite(fraction) || value_ < lo) {
        fraction = 0.0;
    }
    fraction = std::min(1.0, std::max(0.0, fraction));
    if (fraction > 0.0) {
        const auto fill_rows =
            std::max(1LL, std::llround(fraction * static_cast<double>(tube_h)));
        const Style liquid = Style{}.set_fg(level_color(fraction));
        for (long long y = bottom; y > bottom - fill_rows && y < canvas_h; --y) {
            if (y >= 0 && tx >= 0 && tx < canvas_w) {
                ctx.canvas.put(static_cast<std::uint32_t>(tx),
                               static_cast<std::uint32_t>(y), kFill, liquid);
            }
        }
    }

    // 右侧刻度：─ + 数值（空间够才画标签）。
    const int n = std::min(10, std::max(0, options_.ticks));
    const Style tick_st{};
    for (int k = 1; k <= n; ++k) {
        const double frac = static_cast<double>(k) / static_cast<double>(n + 1);
        const long long y = bottom - std::llround(frac * static_cast<double>(tube_h));
        if (y < top || y > bottom || y < 0 || y >= canvas_h) {
            continue;
        }
        const long long tick_x = tx + 1;
        if (tick_x < 0 || tick_x >= canvas_w) {
            continue;
        }
        ctx.canvas.put(static_cast<std::uint32_t>(tick_x),
                       static_cast<std::uint32_t>(y), kTick, tick_st);
        if (tick_x + 2 < static_cast<long long>(area.right())) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.0f", lo + frac * (hi - lo));
            ctx.canvas.draw_text(Point{static_cast<double>(tick_x + 1),
                                       static_cast<double>(y)},
                                 buf, tick_st);
        }
    }
}

}  // namespace termchart::chart
