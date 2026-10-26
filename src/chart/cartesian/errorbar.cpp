// errorbar.cpp —— 误差棒实现（中心点 + 竖/横误差区间）
#include "termchart/chart/cartesian/errorbar.hpp"

#include <algorithm>
#include <cmath>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"
#include "termchart/chart/scene_sink.hpp"

namespace termchart::chart {
namespace {

// kMarkerChar（● 数据点）沿用 chart.hpp 的共享常量。
inline constexpr char32_t kBarVChar = U'\u2502';    // │ 竖向误差线 / 横向端帽
inline constexpr char32_t kBarHChar = U'\u2500';    // ─ 横向误差线 / 竖向端帽

}  // namespace

void ErrorBarChart::draw(DrawContext ctx) {
    // 位置必须有限；误差界非有限 = 该向无误差，lo>hi 自动交换。
    struct Clean {
        Point p{};
        double y_lo = 0.0;
        double y_hi = 0.0;
        double x_lo = 0.0;
        double x_hi = 0.0;
    };
    std::vector<Clean> pts;
    pts.reserve(points_.size());
    for (const ErrorPoint& e : points_) {
        if (!std::isfinite(e.x) || !std::isfinite(e.y)) {
            continue;
        }
        Clean c{};
        c.p = Point{e.x, e.y};
        c.y_lo = std::isfinite(e.y_lo) ? std::min(e.y_lo, e.y) : e.y;
        c.y_hi = std::isfinite(e.y_hi) ? std::max(e.y_hi, e.y) : e.y;
        c.x_lo = std::isfinite(e.x_lo) ? std::min(e.x_lo, e.x) : e.x;
        c.x_hi = std::isfinite(e.x_hi) ? std::max(e.x_hi, e.x) : e.x;
        pts.push_back(c);
    }
    if (pts.empty()) {
        return;
    }

    // 域把误差区间一起包进来，否则端帽会贴边甚至出界。
    double xmin = pts.front().x_lo;
    double xmax = pts.front().x_hi;
    double ymin = pts.front().y_lo;
    double ymax = pts.front().y_hi;
    for (const Clean& c : pts) {
        xmin = std::min(xmin, c.x_lo);
        xmax = std::max(xmax, c.x_hi);
        ymin = std::min(ymin, c.y_lo);
        ymax = std::max(ymax, c.y_hi);
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, xmin, xmax, ymin, ymax, true, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
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

    const Style st = Style{}.set_fg(opts_.color);
    for (const Clean& c : pts) {
        const Point ctr = area_.cell_of(c.p.x, c.p.y);

        if (c.y_hi > c.y_lo) {
            const Point lo = area_.cell_of(c.p.x, c.y_lo);
            const Point hi = area_.cell_of(c.p.x, c.y_hi);
            ctx.canvas.draw_line(lo, hi, kBarVChar, st);
            if (opts_.show_caps) {
                ctx.canvas.fill_rect(Rect{ctr.x - 1.0, lo.y, 3.0, 1.0}, kBarHChar, st);
                ctx.canvas.fill_rect(Rect{ctr.x - 1.0, hi.y, 3.0, 1.0}, kBarHChar, st);
            }
        }
        if (c.x_hi > c.x_lo) {
            const Point lo = area_.cell_of(c.x_lo, c.p.y);
            const Point hi = area_.cell_of(c.x_hi, c.p.y);
            ctx.canvas.draw_line(lo, hi, kBarHChar, st);
            if (opts_.show_caps) {
                ctx.canvas.fill_rect(Rect{lo.x, ctr.y, 1.0, 1.0}, kBarVChar, st);
                ctx.canvas.fill_rect(Rect{hi.x, ctr.y, 1.0, 1.0}, kBarVChar, st);
            }
        }
        ctx.canvas.put(ctr, kMarkerChar, st);
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, area_.inner_rect());
    }
}

bool ErrorBarChart::to_scene(scene::Scene& out, std::uint32_t width,
                              std::uint32_t height) const {
    // 与 draw() 同源：位置必须有限；误差界非有限 = 该向无误差，lo>hi 自动交换。
    struct Clean {
        Point p{};
        double y_lo = 0.0;
        double y_hi = 0.0;
        double x_lo = 0.0;
        double x_hi = 0.0;
    };
    std::vector<Clean> pts;
    pts.reserve(points_.size());
    for (const ErrorPoint& e : points_) {
        if (!std::isfinite(e.x) || !std::isfinite(e.y)) {
            continue;
        }
        Clean c{};
        c.p = Point{e.x, e.y};
        c.y_lo = std::isfinite(e.y_lo) ? std::min(e.y_lo, e.y) : e.y;
        c.y_hi = std::isfinite(e.y_hi) ? std::max(e.y_hi, e.y) : e.y;
        c.x_lo = std::isfinite(e.x_lo) ? std::min(e.x_lo, e.x) : e.x;
        c.x_hi = std::isfinite(e.x_hi) ? std::max(e.x_hi, e.x) : e.x;
        pts.push_back(c);
    }
    if (pts.empty()) {
        return true;
    }

    // 域把误差区间一起包进来，否则端帽会贴边甚至出界。
    double xmin = pts.front().x_lo;
    double xmax = pts.front().x_hi;
    double ymin = pts.front().y_lo;
    double ymax = pts.front().y_hi;
    for (const Clean& c : pts) {
        xmin = std::min(xmin, c.x_lo);
        xmax = std::max(xmax, c.x_hi);
        ymin = std::min(ymin, c.y_lo);
        ymax = std::max(ymax, c.y_hi);
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(width, height, m, xmin, xmax, ymin, ymax, true, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());

    SceneSink sink(out);
    layout::draw_grid(sink, area_, xticks, yticks,
                      Style{}.set_fg(default_color(DefaultColor::Muted)));
    layout::draw_axis_x(sink, area_, xticks, "x", Style{});
    layout::draw_axis_y(sink, area_, yticks, "y", Style{});

    const Style st = Style{}.set_fg(opts_.color);
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);
    for (const Clean& c : pts) {
        const Point ctr = area_.cell_of(c.p.x, c.p.y);

        if (c.y_hi > c.y_lo) {
            const Point lo = area_.cell_of(c.p.x, c.y_lo);
            const Point hi = area_.cell_of(c.p.x, c.y_hi);
            b.stroke(st.fg());
            b.move_to(lo);
            b.line_to(hi);
            if (opts_.show_caps) {
                b.fill(st.fg());
                b.rect(Rect{ctr.x - 1.0, lo.y, 3.0, 1.0});
                b.fill(st.fg());
                b.rect(Rect{ctr.x - 1.0, hi.y, 3.0, 1.0});
            }
        }
        if (c.x_hi > c.x_lo) {
            const Point lo = area_.cell_of(c.x_lo, c.p.y);
            const Point hi = area_.cell_of(c.x_hi, c.p.y);
            b.stroke(st.fg());
            b.move_to(lo);
            b.line_to(hi);
            if (opts_.show_caps) {
                b.fill(st.fg());
                b.rect(Rect{lo.x, ctr.y, 1.0, 1.0});
                b.fill(st.fg());
                b.rect(Rect{hi.x, ctr.y, 1.0, 1.0});
            }
        }
        sink.put(ctr, kMarkerChar, st);
    }
    b.end_group(0);

    if (!title_.empty()) {
        layout::draw_title(sink, title_, area_.inner_rect());
    }
    return true;
}

}  // namespace termchart::chart
