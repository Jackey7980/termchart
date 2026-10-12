// bubble.cpp —— 气泡图实现（半径 → 字符大小阶梯）
#include "termchart/chart/cartesian/bubble.hpp"

#include <algorithm>
#include <cmath>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/legend.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

inline constexpr char32_t kSmallBubble = U'\u00B7';   // ·
inline constexpr char32_t kMediumBubble = U'\u2022';  // •
inline constexpr char32_t kLargeBubble = U'\u25CF';   // ●

char32_t glyph_for(double t) noexcept {
    if (t < 1.0 / 3.0) {
        return kSmallBubble;
    }
    if (t < 2.0 / 3.0) {
        return kMediumBubble;
    }
    return kLargeBubble;
}

}  // namespace

void BubbleChart::draw(DrawContext ctx) {
    // 只保留位置有限的气泡；r 非有限/负按 0 处理（画最小点，不整块丢弃）。
    std::vector<Bubble> pts;
    pts.reserve(bubbles_.size());
    for (const Bubble& b : bubbles_) {
        if (std::isfinite(b.p.x) && std::isfinite(b.p.y)) {
            pts.push_back(Bubble{b.p, (std::isfinite(b.r) && b.r > 0.0) ? b.r : 0.0});
        }
    }
    if (pts.empty()) {
        return;
    }

    double xmin = pts.front().p.x;
    double xmax = xmin;
    double ymin = pts.front().p.y;
    double ymax = ymin;
    double rmin = pts.front().r;
    double rmax = rmin;
    for (const Bubble& b : pts) {
        xmin = std::min(xmin, b.p.x);
        xmax = std::max(xmax, b.p.x);
        ymin = std::min(ymin, b.p.y);
        ymax = std::max(ymax, b.p.y);
        rmin = std::min(rmin, b.r);
        rmax = std::max(rmax, b.r);
    }
    const double rspan = rmax - rmin;

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, xmin, xmax, ymin, ymax, true, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    const Style axis_style = ctx.axis_style;
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(Color::ansi16(8))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "x", axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "y", axis_style);
    }

    const Style st = Style{}.set_fg(opts_.color);
    for (const Bubble& b : pts) {
        // 全体同值（rspan==0）时第三维无信息，取中档。
        const double t = rspan > 0.0 ? (b.r - rmin) / rspan : 0.5;
        ctx.canvas.put(area_.cell_of(b.p.x, b.p.y), glyph_for(t), st);
    }

    if (opts_.show_legend && ctx.draw_legend) {
        const std::vector<layout::LegendEntry> entries{
            layout::LegendEntry{opts_.name, opts_.color}};
        const Rect inner = area_.inner_rect();
        layout::draw_legend(ctx.canvas, entries, Rect{inner.x, 0.0, inner.w, 1.0},
                            layout::HAlign::Left);
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, area_.inner_rect());
    }
}

}  // namespace termchart::chart
