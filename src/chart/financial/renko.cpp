// renko.cpp —— 砖形图实现：阈值过档生成砖 + 阶梯排布
#include "termchart/chart/financial/renko.hpp"

#include <algorithm>
#include <vector>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

struct Brick {
    int level = 0;  // 砖底所在档位（一档 = brick_size）
    bool up = true;
};

std::vector<Brick> build_bricks(const std::vector<double>& prices, double size) {
    std::vector<Brick> bricks;
    if (prices.size() < 2 || size <= 0.0) {
        return bricks;
    }
    double ref = prices.front();
    int level = 0;
    for (double p : prices) {
        while (p >= ref + size) {
            bricks.push_back(Brick{level, true});
            ref += size;
            level += 1;
        }
        while (p <= ref - size) {
            level -= 1;
            bricks.push_back(Brick{level, false});
            ref -= size;
        }
    }
    return bricks;
}

}  // namespace

void RenkoChart::set_prices(std::vector<double> prices) {
    prices_ = std::move(prices);
}

void RenkoChart::draw(DrawContext ctx) {
    double pmin = 0.0;
    double pmax = 0.0;
    if (!prices_.empty()) {
        pmin = *std::min_element(prices_.begin(), prices_.end());
        pmax = *std::max_element(prices_.begin(), prices_.end());
    }
    double size = options.brick_size;
    if (size <= 0.0 && pmax > pmin) {
        size = (pmax - pmin) / 10.0;
    }
    const std::vector<Brick> bricks = build_bricks(prices_, size);
    if (bricks.empty()) {
        return;  // 数据不足或全距为 0：无砖可画
    }

    int lv_min = bricks.front().level;
    int lv_max = bricks.front().level;
    for (const Brick& b : bricks) {
        lv_min = std::min(lv_min, b.level);
        lv_max = std::max(lv_max, b.level);
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const double xmin = -0.5;
    const double xmax = static_cast<double>(bricks.size()) - 0.5;
    area_ = make_area(ctx.canvas, m, xmin, xmax,
                      static_cast<double>(lv_min), static_cast<double>(lv_max + 1),
                      false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "brick", ctx.axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "level", ctx.axis_style);
    }

    const Rect inner = area_.inner_rect();
    const double slot = inner.w / static_cast<double>(bricks.size());
    const double bw = slot >= 2.0 ? 2.0 : slot;
    for (std::size_t i = 0; i < bricks.size(); ++i) {
        const Brick& b = bricks[i];
        const double x0 = area_.x_scale().forward(static_cast<double>(i));
        const double top = area_.y_scale().forward(static_cast<double>(b.level + 1));
        const double bottom = area_.y_scale().forward(static_cast<double>(b.level));
        ctx.canvas.fill_rect(Rect{x0, top, bw, bottom - top}, kBarChar,
                             Style{}.set_fg(b.up ? options.up : options.down));
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

}  // namespace termchart::chart
