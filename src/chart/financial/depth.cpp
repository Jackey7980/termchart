// depth.cpp —— 深度图实现：两侧阶梯 + ░ 量程填充
#include "termchart/chart/financial/depth.hpp"

#include <algorithm>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

// 一侧订单簿：阶梯线 • + 下方 ░ 填充。points 按 price 升序传入。
void draw_side(canvas::Canvas& canvas, const layout::PlotArea& area,
               const std::vector<double>& price, const std::vector<double>& qty,
               Color color) {
    const Style st = Style{}.set_fg(color);
    const Rect inner = area.inner_rect();
    for (std::size_t i = 0; i < price.size(); ++i) {
        const double x = area.x_scale().forward(price[i]);
        const double y = area.y_scale().forward(qty[i]);
        canvas.put(Point{x, y}, kLineChar, st);
        const double next_x =
            (i + 1 < price.size())
                ? area.x_scale().forward(price[i + 1])
                : inner.right();
        if (next_x > x) {
            canvas.fill_rect(Rect{x, y, next_x - x, inner.bottom() - y},
                             U'\u2591', st);  // ░
        }
        if (i + 1 < price.size()) {
            // 阶梯竖沿：量跳变处一列补齐，视觉上封住台阶。
            const double y_next = area.y_scale().forward(qty[i + 1]);
            canvas.draw_line(Point{next_x, y}, Point{next_x, y_next}, U'\u2591', st);
        }
    }
}

}  // namespace

void DepthChart::set_book(std::vector<double> bid_price, std::vector<double> bid_qty,
                          std::vector<double> ask_price, std::vector<double> ask_qty) {
    bid_price_ = std::move(bid_price);
    bid_qty_ = std::move(bid_qty);
    ask_price_ = std::move(ask_price);
    ask_qty_ = std::move(ask_qty);
}

void DepthChart::draw(DrawContext ctx) {
    if (bid_price_.empty() || ask_price_.empty()) {
        return;  // 两侧都非空才成"深度"；缺一侧无法成图
    }
    double pmin = bid_price_.front();
    double pmax = bid_price_.back();
    for (double p : bid_price_) {
        pmin = std::min(pmin, p);
        pmax = std::max(pmax, p);
    }
    for (double p : ask_price_) {
        pmin = std::min(pmin, p);
        pmax = std::max(pmax, p);
    }
    double qmax = 0.0;
    for (double q : bid_qty_) {
        qmax = std::max(qmax, q);
    }
    for (double q : ask_qty_) {
        qmax = std::max(qmax, q);
    }
    if (pmax <= pmin || qmax <= 0.0) {
        return;
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const layout::PlotArea area = make_area(ctx.canvas, m, pmin, pmax, 0.0, qmax,
                                            false, false);
    const std::vector<double> xticks = scale::ticks_for(area.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area.y_scale());
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(Color::ansi16(8))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area, xticks, "price", ctx.axis_style);
        layout::draw_axis_y(ctx.canvas, area, yticks, "qty", ctx.axis_style);
    }
    draw_side(ctx.canvas, area, bid_price_, bid_qty_, options.bid);
    draw_side(ctx.canvas, area, ask_price_, ask_qty_, options.ask);
}

}  // namespace termchart::chart
