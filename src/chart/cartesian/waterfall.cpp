// waterfall.cpp —— 瀑布图实现（累计基线 → 新累计值的 █ 柵 + ─ 连接线）
#include "termchart/chart/cartesian/waterfall.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

// kBarChar（█ 柱）沿用 chart.hpp 的共享常量。
inline constexpr char32_t kConnectorChar = U'\u2500';   // ─ 连接线

}  // namespace

void WaterfallChart::set_data(std::vector<Item> items) {
    // 值非有限的条目剔除；累计序列与保留条目一一对应。
    items_.clear();
    cumulative_.clear();
    items_.reserve(items.size());
    cumulative_.reserve(items.size());
    double running = 0.0;
    for (Item& it : items) {
        if (!std::isfinite(it.value)) {
            continue;
        }
        if (it.is_total) {
            running = it.value;
        } else {
            running += it.value;
        }
        cumulative_.push_back(running);
        items_.push_back(std::move(it));
    }
}

void WaterfallChart::draw(DrawContext ctx) {
    if (items_.empty()) {
        return;
    }

    // 先算出每段的 [base, top]，y 域连同 0（基线）一起包进来。
    struct Seg {
        double base = 0.0;
        double top = 0.0;
        bool is_total = false;
        bool rising = false;
    };
    std::vector<Seg> segs;
    segs.reserve(items_.size());
    double running = 0.0;
    double ymin = 0.0;
    double ymax = 0.0;
    for (const Item& it : items_) {
        Seg s;
        s.is_total = it.is_total;
        s.rising = it.is_total || it.value >= 0.0;
        s.base = it.is_total ? 0.0 : running;
        s.top = it.is_total ? it.value : running + it.value;
        running = it.is_total ? it.value : s.top;
        ymin = std::min(ymin, std::min(s.base, s.top));
        ymax = std::max(ymax, std::max(s.base, s.top));
        segs.push_back(s);
    }

    const auto n = static_cast<double>(items_.size());
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, -0.5, n - 0.5, ymin, ymax, false, false);

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
        layout::draw_axis_x(ctx.canvas, area_, xticks, "step", axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "value", axis_style);
    }

    const Rect inner = area_.inner_rect();
    const double bw = std::max(1.0, inner.w / n * 0.6);

    for (std::size_t i = 0; i < segs.size(); ++i) {
        const Seg& s = segs[i];
        const double xc = area_.x_scale().forward(static_cast<double>(i));
        const Color col = s.is_total ? opts_.total
                                     : (s.rising ? opts_.increase : opts_.decrease);
        const Style st = Style{}.set_fg(col);
        const double top_cell = area_.y_scale().forward(std::max(s.base, s.top));
        const double h = std::max(1.0, std::fabs(area_.y_scale().forward(s.base) -
                                                 area_.y_scale().forward(s.top)));
        ctx.canvas.fill_rect(Rect{xc - bw / 2.0, top_cell, bw, h}, kBarChar, st);

        // 连接线：从本段右缘到下一段左缘，画在本段结束的累计高度。
        if (opts_.show_connectors && i + 1 < segs.size()) {
            const double lvl = area_.y_scale().forward(s.top);
            const double xc_next = area_.x_scale().forward(static_cast<double>(i + 1));
            const Style lc = Style{}.set_fg(opts_.total);
            ctx.canvas.draw_line(Point{xc + bw / 2.0, lvl},
                                 Point{xc_next - bw / 2.0, lvl}, kConnectorChar, lc);
        }
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

}  // namespace termchart::chart
