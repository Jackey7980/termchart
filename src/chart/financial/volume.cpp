// volume.cpp —— 成交量图实现：槽位柱 + 方向色
#include "termchart/chart/financial/volume.hpp"

#include <algorithm>
#include <cmath>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {

void VolumeChart::set_values(std::vector<double> values, std::vector<int> dirs) {
    values_ = std::move(values);
    dirs_ = std::move(dirs);
}

void VolumeChart::draw(DrawContext ctx) {
    if (values_.empty()) {
        return;
    }
    double vmax = 0.0;
    for (double v : values_) {
        vmax = std::max(vmax, v);
    }
    if (vmax <= 0.0) {
        return;
    }
    const double xmin = -0.5;
    const double xmax = static_cast<double>(values_.size()) - 0.5;
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, xmin, xmax, 0.0, vmax, true, true);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(Color::ansi16(8))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "t", ctx.axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "volume", ctx.axis_style);
    }

    const Rect inner = area_.inner_rect();
    const double slot = inner.w / static_cast<double>(values_.size());
    const double bw = slot * 0.7;
    const double y0 = area_.y_scale().forward(0.0);
    for (std::size_t i = 0; i < values_.size(); ++i) {
        const double x0 = area_.x_scale().forward(static_cast<double>(i)) - bw / 2.0;
        const double y1 = area_.y_scale().forward(values_[i]);
        Color color = options.neutral;
        if (i < dirs_.size()) {
            color = dirs_[i] > 0 ? options.up
                                 : (dirs_[i] < 0 ? options.down : options.neutral);
        }
        ctx.canvas.fill_rect(Rect{x0, std::min(y0, y1), bw, std::fabs(y1 - y0)},
                             kBarChar, Style{}.set_fg(color));
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

}  // namespace termchart::chart
