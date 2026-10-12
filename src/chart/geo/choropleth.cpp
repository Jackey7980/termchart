// choropleth.cpp —— 分级统计图实现：区域格填色 + 值分级
#include "termchart/chart/geo/choropleth.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/layout/title.hpp"

namespace termchart::chart {
namespace {

constexpr char32_t kBlock = U'\u2588';  // █

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

// 值 → 分级色：灰阶 256 色段（232 最深 .. 255 最浅），值越高越深。
Color ramp_color(double fraction01) noexcept {
    double f = std::isfinite(fraction01) ? fraction01 : 0.0;
    f = std::min(1.0, std::max(0.0, f));
    const auto level = static_cast<std::uint8_t>(23 - std::llround(f * 23.0));
    return Color::indexed(static_cast<std::uint8_t>(232 + level));
}

// 浮点格坐标取整并钳进 [0, limit)；NaN/越界返回 false。
bool round_cell(double v, std::uint32_t limit, std::uint32_t& out) noexcept {
    if (!(v >= 0.0) || !(v < static_cast<double>(limit))) {
        return false;
    }
    const long long r = std::llround(v);
    if (r < 0 || r >= static_cast<long long>(limit)) {
        return false;
    }
    out = static_cast<std::uint32_t>(r);
    return true;
}

}  // namespace

void Choropleth::draw(DrawContext ctx) {
    if (regions_.empty()) {
        return;
    }

    // 非有限值按 0 收敛后再求值域。
    double vmin = 0.0;
    double vmax = 0.0;
    bool have = false;
    for (const GeoRegion& r : regions_) {
        const double v = std::isfinite(r.value) ? r.value : 0.0;
        if (!have) {
            vmin = vmax = v;
            have = true;
        } else {
            vmin = std::min(vmin, v);
            vmax = std::max(vmax, v);
        }
    }

    const Rect area = content_area(ctx, *this);
    const int bins = std::min(8, std::max(1, options_.bins));

    for (const GeoRegion& r : regions_) {
        const double v = std::isfinite(r.value) ? r.value : 0.0;
        // 均分 bins 档：档位取整后再映射回 0..1，保证同档同色。
        double frac = vmax > vmin ? (v - vmin) / (vmax - vmin) : 0.0;
        frac = std::min(1.0, std::max(0.0, frac));
        if (bins > 1) {
            const auto idx = static_cast<int>(std::llround(frac * (bins - 1)));
            frac = static_cast<double>(idx) / static_cast<double>(bins - 1);
        } else {
            frac = 0.0;
        }
        const Style st = Style{}.set_fg(ramp_color(frac));
        for (const Point& cell : r.cells) {
            std::uint32_t cx = 0;
            std::uint32_t cy = 0;
            if (round_cell(cell.x, ctx.canvas.width(), cx) &&
                round_cell(cell.y, ctx.canvas.height(), cy)) {
                ctx.canvas.put(cx, cy, kBlock, st);
            }
        }
    }

    // 图例：内容区底行画 min 色 ─ max 色（空间不足则跳过，不挤主体）。
    if (area.h >= 2.0 && area.w >= 14.0 && vmax > vmin) {
        const double ly = area.bottom() - 1.0;
        ctx.canvas.draw_text(Point{area.x, ly}, "min", Style{});
        ctx.canvas.put(Point{area.x + 3.0, ly}, kBlock, Style{}.set_fg(ramp_color(0.0)));
        ctx.canvas.put(Point{area.x + 4.0, ly}, kBlock, Style{}.set_fg(ramp_color(0.5)));
        ctx.canvas.put(Point{area.x + 5.0, ly}, kBlock, Style{}.set_fg(ramp_color(1.0)));
        char buf[32];
        std::snprintf(buf, sizeof(buf), "max(%.1f)", vmax);
        ctx.canvas.draw_text(Point{area.x + 6.0, ly}, buf, Style{});
    }
}

}  // namespace termchart::chart
