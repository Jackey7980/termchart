// bland_altman.cpp —— 一致性图实现：均值-差散点 + 一致性界限
#include "termchart/chart/statistical/bland_altman.hpp"

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

void BlandAltmanChart::set_pairs(std::vector<double> method_a,
                                 std::vector<double> method_b, Color color,
                                 std::string name) {
    pairs_.clear();
    valid_ = false;
    mean_ = 0.0;
    sd_ = 0.0;

    const std::size_t n = std::min(method_a.size(), method_b.size());
    for (std::size_t i = 0; i < n; ++i) {
        if (std::isfinite(method_a[i]) && std::isfinite(method_b[i])) {
            pairs_.push_back(Point{(method_a[i] + method_b[i]) * 0.5,
                                   method_a[i] - method_b[i]});
        }
    }
    if (pairs_.size() < 2) {
        return;
    }
    double sum = 0.0;
    for (const Point& p : pairs_) {
        sum += p.y;
    }
    mean_ = sum / static_cast<double>(pairs_.size());
    double ss = 0.0;
    for (const Point& p : pairs_) {
        const double d = p.y - mean_;
        ss += d * d;
    }
    sd_ = std::sqrt(ss / static_cast<double>(pairs_.size() - 1));
    valid_ = true;
    options_.color = color;
    name_ = std::move(name);
}

void BlandAltmanChart::draw(DrawContext ctx) {
    if (!valid_) {
        return;
    }
    double xmin = pairs_.front().x;
    double xmax = xmin;
    for (const Point& p : pairs_) {
        xmin = std::min(xmin, p.x);
        xmax = std::max(xmax, p.x);
    }
    if (!(xmax > xmin)) {
        xmin -= 0.5;
        xmax += 0.5;
    }
    double lo = lower_limit();
    double hi = upper_limit();
    for (const Point& p : pairs_) {
        lo = std::min(lo, p.y);
        hi = std::max(hi, p.y);
    }
    if (!(hi > lo)) {
        lo -= 0.5;
        hi += 0.5;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, xmin, xmax, lo, hi, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "mean", ctx.axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "diff", ctx.axis_style);
    }

    const Style st = Style{}.set_fg(options_.color);
    const Rect inner = area_.inner_rect();
    // mean 差实线 + ±1.96·SD 虚线：横贯绘图区。
    const double y_mean = area_.y_scale().forward(mean_);
    ctx.canvas.draw_line(Point{inner.x, y_mean}, Point{inner.right(), y_mean},
                         U'\u2500', st);
    if (options_.show_limits && sd_ > 0.0) {
        const Style band = Style{}.set_fg(default_color(DefaultColor::Muted));
        for (double level : {lower_limit(), upper_limit()}) {
            const double y = area_.y_scale().forward(level);
            ctx.canvas.draw_line(Point{inner.x, y}, Point{inner.right(), y},
                                 U'\u00B7', band);
        }
    }
    for (const Point& p : pairs_) {
        ctx.canvas.put(area_.cell_of(p.x, p.y), kMarkerChar, st);
    }

    if (show_legend_ && ctx.draw_legend && !name_.empty()) {
        const std::vector<layout::LegendEntry> entries{
            layout::LegendEntry{name_, options_.color}};
        const Rect legend_rect{inner.x, 0.0, inner.w, 1.0};
        layout::draw_legend(ctx.canvas, entries, legend_rect, layout::HAlign::Left);
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

bool BlandAltmanChart::to_scene(scene::Scene& out, std::uint32_t width,
                                 std::uint32_t height) const {
    if (!valid_) {
        return true;
    }
    double xmin = pairs_.front().x;
    double xmax = xmin;
    for (const Point& p : pairs_) {
        xmin = std::min(xmin, p.x);
        xmax = std::max(xmax, p.x);
    }
    if (!(xmax > xmin)) {
        xmin -= 0.5;
        xmax += 0.5;
    }
    double lo = lower_limit();
    double hi = upper_limit();
    for (const Point& p : pairs_) {
        lo = std::min(lo, p.y);
        hi = std::max(hi, p.y);
    }
    if (!(hi > lo)) {
        lo -= 0.5;
        hi += 0.5;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(width, height, m, xmin, xmax, lo, hi, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    SceneSink sink(out);
    layout::draw_grid(sink, area_, xticks, yticks,
                      Style{}.set_fg(default_color(DefaultColor::Muted)));
    layout::draw_axis_x(sink, area_, xticks, "mean", Style{});
    layout::draw_axis_y(sink, area_, yticks, "diff", Style{});

    const Style st = Style{}.set_fg(options_.color);
    const Rect inner = area_.inner_rect();
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);

    // mean 差实线 + ±1.96·SD 一致性界限。
    const double y_mean = area_.y_scale().forward(mean_);
    b.stroke(st.fg());
    b.move_to(Point{inner.x, y_mean});
    b.line_to(Point{inner.right(), y_mean});
    if (options_.show_limits && sd_ > 0.0) {
        const double y_lo = area_.y_scale().forward(lower_limit());
        const double y_hi = area_.y_scale().forward(upper_limit());
        b.stroke(default_color(DefaultColor::Muted));
        b.move_to(Point{inner.x, y_lo});
        b.line_to(Point{inner.right(), y_lo});
        b.stroke(default_color(DefaultColor::Muted));
        b.move_to(Point{inner.x, y_hi});
        b.line_to(Point{inner.right(), y_hi});
    }
    for (const Point& p : pairs_) {
        sink.put(area_.cell_of(p.x, p.y), kMarkerChar, st);
    }
    b.end_group(0);

    if (show_legend_ && !name_.empty()) {
        const std::vector<layout::LegendEntry> entries{
            layout::LegendEntry{name_, options_.color}};
        const Rect legend_rect{inner.x, 0.0, inner.w, 1.0};
        layout::draw_legend(sink, entries, legend_rect, layout::HAlign::Left);
    }
    if (!title_.empty()) {
        layout::draw_title(sink, title_, inner);
    }
    return true;
}

}  // namespace termchart::chart
