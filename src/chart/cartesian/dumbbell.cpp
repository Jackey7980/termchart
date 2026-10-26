// dumbbell.cpp —— 哑铃图实现（行内两值连线 + 端点）
#include "termchart/chart/cartesian/dumbbell.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "termchart/canvas/canvas.hpp"
#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"
#include "termchart/chart/scene_sink.hpp"

namespace termchart::chart {

void DumbbellChart::set_rows(std::vector<Row> rows) { rows_ = std::move(rows); }

void DumbbellChart::draw(DrawContext ctx) {
    // 只保留两端都有限的行：NaN/Inf 会污染 x 定义域。
    std::vector<Row> rows;
    rows.reserve(rows_.size());
    for (const Row& r : rows_) {
        if (std::isfinite(r.a) && std::isfinite(r.b)) {
            rows.push_back(r);
        }
    }
    if (rows.empty()) {
        return;
    }

    double xmin = std::min(rows.front().a, rows.front().b);
    double xmax = std::max(rows.front().a, rows.front().b);
    for (const Row& r : rows) {
        xmin = std::min(xmin, std::min(r.a, r.b));
        xmax = std::max(xmax, std::max(r.a, r.b));
    }
    // 端点重合时给单位跨度，避免零宽定义域。
    if (!(xmax > xmin)) {
        xmin -= 0.5;
        xmax += 0.5;
    } else {
        const double pad = (xmax - xmin) * 0.05;  // 端点圆点不顶到边框
        xmin -= pad;
        xmax += pad;
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    // y 定义域 [0, 行数]：第 i 行中心落在 i + 0.5，行与格对齐。
    area_ = make_area(ctx.canvas, m, xmin, xmax, 0.0, static_cast<double>(rows.size()),
                      false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());

    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, {}, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "value", ctx.axis_style);
        layout::draw_axis_y(ctx.canvas, area_, {}, "", ctx.axis_style);
    }

    const Style line_st = Style{}.set_fg(options_.line_color);
    const Style dot_st = Style{}.set_fg(options_.dot_color);
    const Rect inner = area_.inner_rect();

    for (std::size_t i = 0; i < rows.size(); ++i) {
        const Row& r = rows[i];
        const double y = area_.y_scale().forward(static_cast<double>(i) + 0.5);
        const double x0 = area_.x_scale().forward(std::min(r.a, r.b));
        const double x1 = area_.x_scale().forward(std::max(r.a, r.b));
        ctx.canvas.draw_line(Point{x0, y}, Point{x1, y}, kLineChar, line_st);
        ctx.canvas.put(Point{x0, y}, kMarkerChar, dot_st);
        ctx.canvas.put(Point{x1, y}, kMarkerChar, dot_st);
        if (!r.label.empty()) {
            ctx.canvas.draw_text(Point{0.0, y}, r.label, Style{});
        }
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

bool DumbbellChart::to_scene(scene::Scene& out, std::uint32_t width,
                              std::uint32_t height) const {
    // 与 draw() 同源：只保留两端都有限的行。
    std::vector<Row> rows;
    rows.reserve(rows_.size());
    for (const Row& r : rows_) {
        if (std::isfinite(r.a) && std::isfinite(r.b)) {
            rows.push_back(r);
        }
    }
    if (rows.empty()) {
        return true;
    }

    double xmin = std::min(rows.front().a, rows.front().b);
    double xmax = std::max(rows.front().a, rows.front().b);
    for (const Row& r : rows) {
        xmin = std::min(xmin, std::min(r.a, r.b));
        xmax = std::max(xmax, std::max(r.a, r.b));
    }
    // 端点重合时给单位跨度，避免零宽定义域。
    if (!(xmax > xmin)) {
        xmin -= 0.5;
        xmax += 0.5;
    } else {
        const double pad = (xmax - xmin) * 0.05;  // 端点圆点不顶到边框
        xmin -= pad;
        xmax += pad;
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    // y 定义域 [0, 行数]：第 i 行中心落在 i + 0.5，行与格对齐。
    area_ = make_area(width, height, m, xmin, xmax, 0.0,
                      static_cast<double>(rows.size()), false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());

    SceneSink sink(out);
    layout::draw_grid(sink, area_, xticks, {},
                      Style{}.set_fg(default_color(DefaultColor::Muted)));
    layout::draw_axis_x(sink, area_, xticks, "value", Style{});
    layout::draw_axis_y(sink, area_, {}, "", Style{});

    const Style line_st = Style{}.set_fg(options_.line_color);
    const Style dot_st = Style{}.set_fg(options_.dot_color);
    const Rect inner = area_.inner_rect();
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const Row& r = rows[i];
        const double y = area_.y_scale().forward(static_cast<double>(i) + 0.5);
        const double x0 = area_.x_scale().forward(std::min(r.a, r.b));
        const double x1 = area_.x_scale().forward(std::max(r.a, r.b));
        b.stroke(line_st.fg());
        b.move_to(Point{x0, y});
        b.line_to(Point{x1, y});
        sink.put(Point{x0, y}, kMarkerChar, dot_st);
        sink.put(Point{x1, y}, kMarkerChar, dot_st);
        if (!r.label.empty()) {
            sink.text(Point{0.0, y}, r.label, Style{});
        }
    }
    b.end_group(0);

    if (!title_.empty()) {
        layout::draw_title(sink, title_, inner);
    }
    return true;
}

}  // namespace termchart::chart
