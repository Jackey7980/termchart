// regression.cpp —— 线性回归实现：闭式 OLS + 残差竖线
#include "termchart/chart/statistical/regression.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "termchart/chart/scene_sink.hpp"

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/legend.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

// 闭式 OLS：b = cov(x,y)/var(x)，a = ȳ - b·x̄。返回是否可拟合。
bool ols(const std::vector<Point>& pts, double& slope, double& intercept,
         double& r_squared) {
    const std::size_t n = pts.size();
    if (n < 2) {
        return false;
    }
    double sx = 0.0;
    double sy = 0.0;
    for (const Point& p : pts) {
        sx += p.x;
        sy += p.y;
    }
    const double mx = sx / static_cast<double>(n);
    const double my = sy / static_cast<double>(n);
    double sxx = 0.0;
    double sxy = 0.0;
    double syy = 0.0;
    for (const Point& p : pts) {
        const double dx = p.x - mx;
        const double dy = p.y - my;
        sxx += dx * dx;
        sxy += dx * dy;
        syy += dy * dy;
    }
    if (!(sxx > 0.0)) {
        return false;  // x 无方差：斜率无定义
    }
    slope = sxy / sxx;
    intercept = my - slope * mx;
    r_squared = (syy > 0.0) ? (sxy * sxy) / (sxx * syy) : 0.0;
    return true;
}

}  // namespace

void RegressionChart::set_points(std::vector<Point> points, Color color,
                                 std::string name) {
    points_.clear();
    points_.reserve(points.size());
    for (const Point& p : points) {
        if (std::isfinite(p.x) && std::isfinite(p.y)) {
            points_.push_back(p);
        }
    }
    color_ = color;
    name_ = std::move(name);
    fitted_ = ols(points_, slope_, intercept_, r_squared_);
}

void RegressionChart::draw(DrawContext ctx) {
    if (points_.empty()) {
        return;
    }
    double xmin = points_.front().x;
    double xmax = xmin;
    double ymin = points_.front().y;
    double ymax = ymin;
    for (const Point& p : points_) {
        xmin = std::min(xmin, p.x);
        xmax = std::max(xmax, p.x);
        ymin = std::min(ymin, p.y);
        ymax = std::max(ymax, p.y);
    }
    if (!(xmax > xmin)) {
        xmin -= 0.5;
        xmax += 0.5;
    }
    if (!(ymax > ymin)) {
        ymin -= 0.5;
        ymax += 0.5;
    }
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
        layout::draw_axis_x(ctx.canvas, area_, xticks, "x", ctx.axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "y", ctx.axis_style);
    }

    const Style st = Style{}.set_fg(color_);
    const Rect inner = area_.inner_rect();
    if (fitted_) {
        // 拟合线：跨全绘图区的直线（clamped scale 保证落格安全）。
        const Point left{inner.x, area_.y_scale().forward(intercept_ + slope_ * area_.x_scale().inverse(inner.x))};
        const Point right{inner.right(), area_.y_scale().forward(intercept_ + slope_ * area_.x_scale().inverse(inner.right()))};
        ctx.canvas.draw_line(left, right, U'\u2500', st);
    }
    for (const Point& p : points_) {
        const Point cell = area_.cell_of(p.x, p.y);
        if (fitted_ && options_.show_residuals) {
            const Point resid = area_.cell_of(p.x, intercept_ + slope_ * p.x);
            if (!(resid == cell)) {  // 零残差不落点：避免把残差线画成假散点
                ctx.canvas.draw_line(cell, resid, U'\u00B7', st);
            }
        }
        ctx.canvas.put(cell, kMarkerChar, st);
    }

    if (show_legend_ && ctx.draw_legend && !name_.empty()) {
        const std::vector<layout::LegendEntry> entries{
            layout::LegendEntry{name_, color_}};
        const Rect legend_rect{inner.x, 0.0, inner.w, 1.0};
        layout::draw_legend(ctx.canvas, entries, legend_rect, layout::HAlign::Left);
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

bool RegressionChart::to_scene(scene::Scene& out, std::uint32_t width,
                               std::uint32_t height) const {
    if (points_.empty()) {
        return true;
    }
    double xmin = points_.front().x;
    double xmax = xmin;
    double ymin = points_.front().y;
    double ymax = ymin;
    for (const Point& p : points_) {
        xmin = std::min(xmin, p.x);
        xmax = std::max(xmax, p.x);
        ymin = std::min(ymin, p.y);
        ymax = std::max(ymax, p.y);
    }
    if (!(xmax > xmin)) {
        xmin -= 0.5;
        xmax += 0.5;
    }
    if (!(ymax > ymin)) {
        ymin -= 0.5;
        ymax += 0.5;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(width, height, m, xmin, xmax, ymin, ymax, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    SceneSink sink(out);
    layout::draw_grid(sink, area_, xticks, yticks,
                      Style{}.set_fg(default_color(DefaultColor::Muted)));
    layout::draw_axis_x(sink, area_, xticks, "x", Style{});
    layout::draw_axis_y(sink, area_, yticks, "y", Style{});

    const Style st = Style{}.set_fg(color_);
    const Rect inner = area_.inner_rect();
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);

    if (fitted_) {
        const Point left{inner.x, area_.y_scale().forward(
                                     intercept_ + slope_ * area_.x_scale().inverse(inner.x))};
        const Point right{inner.right(), area_.y_scale().forward(
                                            intercept_ + slope_ * area_.x_scale().inverse(inner.right()))};
        b.stroke(st.fg());
        b.move_to(left);
        b.line_to(right);
    }
    for (const Point& p : points_) {
        const Point cell = area_.cell_of(p.x, p.y);
        if (fitted_ && options_.show_residuals) {
            const Point resid = area_.cell_of(p.x, intercept_ + slope_ * p.x);
            if (!(resid == cell)) {
                b.stroke(st.fg());
                b.move_to(cell);
                b.line_to(resid);
            }
        }
        sink.put(cell, kMarkerChar, st);
    }
    b.end_group(0);

    if (show_legend_ && !name_.empty()) {
        const std::vector<layout::LegendEntry> entries{
            layout::LegendEntry{name_, color_}};
        const Rect legend_rect{inner.x, 0.0, inner.w, 1.0};
        layout::draw_legend(sink, entries, legend_rect, layout::HAlign::Left);
    }
    if (!title_.empty()) {
        layout::draw_title(sink, title_, inner);
    }
    return true;
}

}  // namespace termchart::chart
