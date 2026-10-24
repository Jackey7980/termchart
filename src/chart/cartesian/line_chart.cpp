// line_chart.cpp —— 笛卡尔折线图实现
#include "termchart/chart/cartesian/line_chart.hpp"

#include <algorithm>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {

void LineChart::draw(DrawContext ctx) {
    double xmin, xmax, ymin, ymax;
    data_bounds(series_, xmin, xmax, ymin, ymax);
    if (!std::isfinite(xmin)) {  // 无数据：空域
        xmin = 0.0;
        xmax = 1.0;
        ymin = 0.0;
        ymax = 1.0;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, xmin, xmax, ymin, ymax, auto_domain_,
                      include_zero_, y_domain_);

    // 显式刻度优先：调用方知道 x 的语义（例如时间），能给出日历对齐的位置；
    // 否则退回 nice 值。这里只做"用哪一份"的选择，不掺任何语义判断。
    const std::vector<double> xticks =
        x_ticks_.empty() ? scale::ticks_for(area_.x_scale()) : x_ticks_;
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());

    // 上下文开关在这里落地：默认全开，导出/GUI 侧可以只要数据不要轴。
    const Style axis_style = ctx.axis_style;
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        if (x_tick_formatter_) {
            std::vector<std::string> labels;
            labels.reserve(xticks.size());
            for (const double t : xticks) {
                labels.push_back(x_tick_formatter_(t));
            }
            layout::draw_axis_x(ctx.canvas, area_, xticks, labels, "x", axis_style);
        } else {
            layout::draw_axis_x(ctx.canvas, area_, xticks, "x", axis_style);
        }
        layout::draw_axis_y(ctx.canvas, area_, yticks, "y", axis_style);
    }

    for (const Series& s : series_) {
        const Style st = Style{}.set_fg(s.color);
        if (s.points.size() >= 2) {
            std::vector<Point> pts = s.points;
            std::sort(pts.begin(), pts.end(),
                      [](const Point& a, const Point& b) { return a.x < b.x; });
            for (std::size_t i = 1; i < pts.size(); ++i) {
                const Point a = area_.cell_of(pts[i - 1].x, pts[i - 1].y);
                const Point b = area_.cell_of(pts[i].x, pts[i].y);
                ctx.canvas.draw_line(a, b, kLineChar, st);
            }
        }
        for (const Point& p : s.points) {
            const Point c = area_.cell_of(p.x, p.y);
            ctx.canvas.put(c, kMarkerChar, st);
        }
    }

    draw_legend_for(ctx.canvas, series_, area_, show_legend_ && ctx.draw_legend);
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, area_.inner_rect());
    }
}

}  // namespace termchart::chart
