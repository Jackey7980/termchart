// area.cpp —— 笛卡尔面积图实现（折线轮廓 + 逐列填充到基线）
#include "termchart/chart/cartesian/area.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include "termchart/canvas/canvas.hpp"
#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/legend.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

// 曲线在数据坐标 data_x 处的高度：在相邻点间线性插值。要求 pts 按 x 升序，
// 且 data_x 落在 [pts.front().x, pts.back().x]（调用方保证，避免域外外推）。
double curve_at(const std::vector<Point>& pts, double data_x) noexcept {
    const auto it = std::upper_bound(
        pts.begin(), pts.end(), data_x,
        [](double value, const Point& p) noexcept { return value < p.x; });
    if (it == pts.begin()) {
        return pts.front().y;
    }
    if (it == pts.end()) {
        return pts.back().y;
    }
    const Point& hi = *it;
    const Point& lo = *(it - 1);
    const double dx = hi.x - lo.x;
    if (dx == 0.0) {
        return hi.y;  // 同 x 的重叠点：取后者，不参与斜率
    }
    const double t = (data_x - lo.x) / dx;
    return lo.y + (hi.y - lo.y) * t;
}

}  // namespace

void AreaChart::set_values(std::vector<Point> points, Color color, std::string name) {
    points_ = std::move(points);
    color_ = color;
    name_ = std::move(name);
}

void AreaChart::draw(DrawContext ctx) {
    // 只保留有限点：NaN/Inf 会让排序比较失去严格弱序、也会污染定义域。先剔除
    // 比"后续处处防空"更省事，也直接满足"NaN 不崩"。
    std::vector<Point> pts;
    pts.reserve(points_.size());
    for (const Point& p : points_) {
        if (std::isfinite(p.x) && std::isfinite(p.y)) {
            pts.push_back(p);
        }
    }
    std::sort(pts.begin(), pts.end(),
              [](const Point& a, const Point& b) noexcept { return a.x < b.x; });

    // 定义域：无数据时退化为单位正方形；有数据时把 baseline 一并纳入 y 域，
    // 否则 fill 到基线的那些格会被比例尺 clamp 掉、填充被截断。
    double xmin = 0.0;
    double xmax = 1.0;
    double ymin = std::min(0.0, baseline_);
    double ymax = std::max(0.0, baseline_);
    if (!pts.empty()) {
        xmin = xmax = pts.front().x;
        ymin = ymax = pts.front().y;
        for (const Point& p : pts) {
            xmin = std::min(xmin, p.x);
            xmax = std::max(xmax, p.x);
            ymin = std::min(ymin, p.y);
            ymax = std::max(ymax, p.y);
        }
        ymin = std::min(ymin, baseline_);
        ymax = std::max(ymax, baseline_);
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, xmin, xmax, ymin, ymax, true, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());

    // 上下文开关照既有图种落地：默认全开，导出/GUI 侧可以只要数据不要轴。
    const Style axis_style = ctx.axis_style;
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "x", axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "y", axis_style);
    }

    const Style st = Style{}.set_fg(color_);
    const Rect inner = area_.inner_rect();

    // 逐列填充：面积 = 每列 [曲线高度, 基线) 竖条的并集。按列算与格粒度画布
    // 天然对齐，直接复用 canvas 的 fill_rect（不自写扫描线/多边形填充）。
    if (pts.size() >= 2) {
        const double ybase = area_.y_scale().forward(baseline_);
        const auto col_begin = static_cast<std::int64_t>(std::floor(inner.x));
        const auto col_end = static_cast<std::int64_t>(std::floor(inner.right()));
        for (std::int64_t col = col_begin; col < col_end; ++col) {
            // 取列中心反推数据 x：避开列边界上"踩点"的歧义。
            const double data_x = area_.x_scale().inverse(static_cast<double>(col) + 0.5);
            if (data_x < pts.front().x || data_x > pts.back().x) {
                continue;  // 曲线只在首末点之间定义，域外不填
            }
            const double ycurve = area_.y_scale().forward(curve_at(pts, data_x));
            const double top = std::min(ycurve, ybase);
            const double bottom = std::max(ycurve, ybase);
            if (!(bottom > top)) {
                continue;  // 与基线同高（或 NaN）：没有可填的高度
            }
            ctx.canvas.fill_rect(Rect{static_cast<double>(col), top, 1.0, bottom - top},
                                 fill_char_, st);
        }
    }

    // 后描线：先填后描，填充绝不盖住折线与数据点。
    if (pts.size() >= 2) {
        for (std::size_t i = 1; i < pts.size(); ++i) {
            const Point a = area_.cell_of(pts[i - 1].x, pts[i - 1].y);
            const Point b = area_.cell_of(pts[i].x, pts[i].y);
            ctx.canvas.draw_line(a, b, kLineChar, st);
        }
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
