// step.cpp —— 笛卡尔阶梯图实现（"横—竖—横"台阶，不退化成折线）
#include "termchart/chart/cartesian/step.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "termchart/canvas/canvas.hpp"
#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/legend.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

// 在两个相邻点（格坐标 a → b）之间画台阶。分档只在"折点位置"上不同：
//   Post：横在 a 的高度、竖在 b 的列；Pre：竖在 a 的列、横在 b 的高度；
//   Mid：横在两点高度中点、两侧各一条半高竖线。
// 一律走 canvas.draw_line（自带裁剪+向下取整），不自己拼格。
void draw_step(canvas::Canvas& canvas, const Point& a, const Point& b,
               StepChart::Where where, char32_t cp, const Style& st) noexcept {
    switch (where) {
        case StepChart::Where::Pre:
            canvas.draw_line(a, Point{a.x, b.y}, cp, st);
            canvas.draw_line(Point{a.x, b.y}, b, cp, st);
            break;
        case StepChart::Where::Mid: {
            const double mid = (a.y + b.y) / 2.0;
            canvas.draw_line(a, Point{a.x, mid}, cp, st);
            canvas.draw_line(Point{a.x, mid}, Point{b.x, mid}, cp, st);
            canvas.draw_line(Point{b.x, mid}, b, cp, st);
            break;
        }
        case StepChart::Where::Post:
        default:
            canvas.draw_line(a, Point{b.x, a.y}, cp, st);
            canvas.draw_line(Point{b.x, a.y}, b, cp, st);
            break;
    }
}

}  // namespace

void StepChart::set_values(std::vector<Point> points, Color color, std::string name) {
    points_ = std::move(points);
    color_ = color;
    name_ = std::move(name);
}

void StepChart::draw(DrawContext ctx) {
    // 只保留有限点：NaN/Inf 会让排序比较失去严格弱序，直接剔除更省事。
    std::vector<Point> pts;
    pts.reserve(points_.size());
    for (const Point& p : points_) {
        if (std::isfinite(p.x) && std::isfinite(p.y)) {
            pts.push_back(p);
        }
    }
    std::sort(pts.begin(), pts.end(),
              [](const Point& a, const Point& b) noexcept { return a.x < b.x; });

    double xmin = 0.0;
    double xmax = 1.0;
    double ymin = 0.0;
    double ymax = 1.0;
    if (!pts.empty()) {
        xmin = xmax = pts.front().x;
        ymin = ymax = pts.front().y;
        for (const Point& p : pts) {
            xmin = std::min(xmin, p.x);
            xmax = std::max(xmax, p.x);
            ymin = std::min(ymin, p.y);
            ymax = std::max(ymax, p.y);
        }
    }

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

    const Style st = Style{}.set_fg(color_);
    const Rect inner = area_.inner_rect();

    for (std::size_t i = 1; i < pts.size(); ++i) {
        const Point a = area_.cell_of(pts[i - 1].x, pts[i - 1].y);
        const Point b = area_.cell_of(pts[i].x, pts[i].y);
        draw_step(ctx.canvas, a, b, where_, kLineChar, st);
    }
    for (const Point& p : pts) {
        ctx.canvas.put(area_.cell_of(p.x, p.y), kMarkerChar, st);
    }

    if (!pts.empty() && show_legend_ && ctx.draw_legend) {
        const std::vector<layout::LegendEntry> entries{layout::LegendEntry{name_, color_}};
        layout::draw_legend(ctx.canvas, entries, Rect{inner.x, 0.0, inner.w, 1.0},
                            layout::HAlign::Left);
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

}  // namespace termchart::chart
