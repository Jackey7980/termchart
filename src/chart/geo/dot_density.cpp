// dot_density.cpp —— 点密度实现：值/unit 个点按序填进区域格
#include "termchart/chart/geo/dot_density.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/layout/title.hpp"

namespace termchart::chart {
namespace {

constexpr char32_t kDot = U'\u2022';  // •

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

void DotDensity::draw(DrawContext ctx) {
    if (regions_.empty()) {
        return;
    }
    content_area(ctx, *this);  // 保留副作用：绘制标题（返回值本方法内未用于裁剪）
    // unit 收敛到 > 0（非有限或 <= 0 都按 1 处理，避免除零）。
    const double unit =
        (std::isfinite(options_.unit) && options_.unit > 0.0) ? options_.unit : 1.0;

    for (const DotDensityRegion& r : regions_) {
        const double value = std::isfinite(r.value) ? r.value : 0.0;
        // 点数 = round(value / unit)，截断到格数；value <= 0 一个点都不画。
        long long dots = 0;
        if (value > 0.0) {
            dots = std::llround(value / unit);
        }
        const auto n = static_cast<std::size_t>(std::min<long long>(
            dots, static_cast<long long>(r.cells.size())));
        const Style st = Style{}.set_fg(r.color);
        for (std::size_t i = 0; i < n; ++i) {
            std::uint32_t cx = 0;
            std::uint32_t cy = 0;
            if (round_cell(r.cells[i].x, ctx.canvas.width(), cx) &&
                round_cell(r.cells[i].y, ctx.canvas.height(), cy)) {
                ctx.canvas.put(cx, cy, kDot, st);
            }
        }
    }
}

}  // namespace termchart::chart
