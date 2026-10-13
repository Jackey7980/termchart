// yield_curve.cpp —— 收益率曲线实现：升序重排 + 连线 + 观测点
#include "termchart/chart/financial/yield_curve.hpp"

#include <algorithm>
#include <cmath>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {

void YieldCurveChart::set_curve(std::vector<double> maturities, std::vector<double> yields,
                                Color color, std::string name) {
    color_ = color;
    name_ = std::move(name);
    points_.clear();

    const std::size_t count = std::min(maturities.size(), yields.size());
    points_.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (!std::isfinite(maturities[i]) || !std::isfinite(yields[i])) {
            continue;  // 非有限观测剔除
        }
        points_.push_back(Point{maturities[i], yields[i]});
    }
    // 按期限升序；重复期限取首见（stable_sort 保序后 unique-erase）。
    std::stable_sort(points_.begin(), points_.end(),
                     [](const Point& a, const Point& b) { return a.x < b.x; });
    points_.erase(std::unique(points_.begin(), points_.end(),
                              [](const Point& a, const Point& b) { return a.x == b.x; }),
                  points_.end());
}

void YieldCurveChart::draw(DrawContext ctx) {
    if (points_.empty()) {
        return;
    }
    double xmin = points_.front().x;
    double xmax = points_.back().x;
    double ymin = points_.front().y;
    double ymax = points_.back().y;
    for (const Point& p : points_) {
        xmin = std::min(xmin, p.x);
        xmax = std::max(xmax, p.x);
        ymin = std::min(ymin, p.y);
        ymax = std::max(ymax, p.y);
    }
    // 退化区间各方向垫一格，避免 make_area 收到零跨度。
    if (xmax - xmin <= 0.0) {
        xmin -= 1.0;
        xmax += 1.0;
    }
    if (ymax - ymin <= 0.0) {
        ymin -= 0.5;
        ymax += 0.5;
    }
    const double pad_y = (ymax - ymin) * 0.05;
    ymin -= pad_y;
    ymax += pad_y;

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, xmin, xmax, ymin, ymax, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "maturity", ctx.axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, name_, ctx.axis_style);
    }

    const Style line_style = Style{}.set_fg(color_);
    auto to_canvas = [&](const Point& p) {
        return Point{area_.x_scale().forward(p.x), area_.y_scale().forward(p.y)};
    };

    // 点间连线（单点时只有标记）。
    if (points_.size() >= 2) {
        for (std::size_t i = 0; i + 1 < points_.size(); ++i) {
            ctx.canvas.draw_line(to_canvas(points_[i]), to_canvas(points_[i + 1]), kLineChar,
                                 line_style);
        }
    }
    if (options_.show_markers) {
        for (const Point& p : points_) {
            ctx.canvas.put(to_canvas(p), kMarkerChar, line_style);
        }
    }

    const Rect inner = area_.inner_rect();
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
    (void)show_legend_;
}

}  // namespace termchart::chart
