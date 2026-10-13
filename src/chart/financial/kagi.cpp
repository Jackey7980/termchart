// kagi.cpp —— 卡吉图实现：反转阈值抽 zigzag 摆动 + 逐列竖线
#include "termchart/chart/financial/kagi.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

// 阈值反转抽摆动：pivots 是 zigzag 极值序列（首元素为首个价格）。
// dir >= 0 视作向上延续：新高则抬升当前极值，回落超过阈值则封顶开新摆。
std::vector<double> build_pivots(const std::vector<double>& prices,
                                 double reversal) {
    std::vector<double> pivots{prices.front()};
    double ext = prices.front();
    int dir = 0;
    for (std::size_t i = 1; i < prices.size(); ++i) {
        const double p = prices[i];
        if (dir >= 0) {
            if (p > ext) {
                ext = p;
                pivots.back() = p;
                dir = 1;
            } else if (p <= ext - reversal) {
                pivots.push_back(p);
                ext = p;
                dir = -1;
            }
        } else {
            if (p < ext) {
                ext = p;
                pivots.back() = p;
                dir = -1;
            } else if (p >= ext + reversal) {
                pivots.push_back(p);
                ext = p;
                dir = 1;
            }
        }
    }
    return pivots;
}

}  // namespace

void KagiChart::set_prices(std::vector<double> prices) {
    prices_ = std::move(prices);
}

void KagiChart::draw(DrawContext ctx) {
    std::vector<double> finite;
    finite.reserve(prices_.size());
    for (double p : prices_) {
        if (std::isfinite(p)) {
            finite.push_back(p);
        }
    }
    if (finite.size() < 2) {
        return;  // 无摆动可言
    }
    const auto [pmin_it, pmax_it] = std::minmax_element(finite.begin(), finite.end());
    const double range = *pmax_it - *pmin_it;
    if (!(range > 0.0)) {
        return;
    }
    const double reversal = options.reversal > 0.0 ? options.reversal : range * 0.05;
    const std::vector<double> pivots = build_pivots(finite, reversal);
    const std::size_t columns = pivots.size() - 1;
    if (columns == 0) {
        return;  // 全程未越过反转阈值
    }

    double lo = pivots[0];
    double hi = pivots[0];
    for (double p : pivots) {
        lo = std::min(lo, p);
        hi = std::max(hi, p);
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, -0.5, static_cast<double>(columns) - 0.5,
                      lo, hi, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "swing", ctx.axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "price", ctx.axis_style);
    }

    // 阳柱判定：上行柱顶超过前一个上行柱顶（首个上行柱视为阳）。
    double prev_up_top = -std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < columns; ++i) {
        const double a = pivots[i];
        const double b = pivots[i + 1];
        const bool up = b > a;
        const double top = std::max(a, b);
        const double bottom = std::min(a, b);
        const bool yang = up && top > prev_up_top;
        if (up) {
            prev_up_top = std::max(prev_up_top, top);
        }
        const Style st = Style{}.set_fg(yang ? options.yang : options.yin);
        const double x = area_.x_scale().forward(static_cast<double>(i));
        ctx.canvas.draw_line(Point{x, area_.y_scale().forward(bottom)},
                             Point{x, area_.y_scale().forward(top)},
                             yang ? U'\u2503' : U'\u2502', st);  // ┃ / │
        // 转折横线：本柱末价连到下一柱起点。
        if (i + 1 < columns) {
            const double x_next = area_.x_scale().forward(static_cast<double>(i + 1));
            ctx.canvas.draw_line(Point{x, area_.y_scale().forward(b)},
                                 Point{x_next, area_.y_scale().forward(b)},
                                 U'\u2500', st);
        }
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, area_.inner_rect());
    }
}

}  // namespace termchart::chart
