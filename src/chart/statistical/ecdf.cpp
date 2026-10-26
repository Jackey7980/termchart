// ecdf.cpp —— 经验分布函数实现：升序台阶 + 重复值合并抬升
#include "termchart/chart/statistical/ecdf.hpp"

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

void EcdfChart::set_values(std::vector<double> values, Color color, std::string name) {
    sorted_.clear();
    sorted_.reserve(values.size());
    for (double x : values) {
        if (std::isfinite(x)) {
            sorted_.push_back(x);
        }
    }
    std::sort(sorted_.begin(), sorted_.end());
    color_ = color;
    name_ = std::move(name);
}

void EcdfChart::draw(DrawContext ctx) {
    const std::size_t n = sorted_.size();
    if (n == 0) {
        return;
    }
    double xmin = sorted_.front();
    double xmax = sorted_.back();
    if (!(xmax > xmin)) {
        // 单点/全等样本：撑开一格避免零宽域。
        xmin -= 0.5;
        xmax += 0.5;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, xmin, xmax, 0.0, 1.0, false, false);

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
        layout::draw_axis_y(ctx.canvas, area_, yticks, "F(x)", ctx.axis_style);
    }

    const Style st = Style{}.set_fg(color_);
    const Rect inner = area_.inner_rect();
    double prev_x = inner.x;
    double prev_y = area_.y_scale().forward(0.0);
    std::size_t i = 0;
    while (i < n) {
        std::size_t j = i;
        while (j < n && sorted_[j] == sorted_[i]) {
            ++j;  // 重复值合并为一次完整抬升
        }
        const double level = static_cast<double>(j) / static_cast<double>(n);
        const double x = area_.x_scale().forward(sorted_[i]);
        const double y = area_.y_scale().forward(level);
        if (x > prev_x) {
            ctx.canvas.draw_line(Point{prev_x, prev_y}, Point{x, prev_y},
                                 U'\u2500', st);  // ─ 水平段
            if (options_.show_risers) {
                ctx.canvas.draw_line(Point{x, prev_y}, Point{x, y},
                                     U'\u2502', st);  // │ 抬升
            }
        }
        if (options_.show_markers) {
            ctx.canvas.put(Point{x, y}, kMarkerChar, st);
        }
        prev_x = x;
        prev_y = y;
        i = j;
    }
    if (prev_x < inner.right()) {
        // 末级水平延伸到右缘：F̂ 在最大样本之后恒为 1。
        ctx.canvas.draw_line(Point{prev_x, prev_y}, Point{inner.right(), prev_y},
                             U'\u2500', st);
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

bool EcdfChart::to_scene(scene::Scene& out, std::uint32_t width,
                          std::uint32_t height) const {
    const std::size_t n = sorted_.size();
    if (n == 0) {
        return true;
    }
    double xmin = sorted_.front();
    double xmax = sorted_.back();
    if (!(xmax > xmin)) {
        xmin -= 0.5;
        xmax += 0.5;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(width, height, m, xmin, xmax, 0.0, 1.0, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    SceneSink sink(out);
    layout::draw_grid(sink, area_, xticks, yticks,
                      Style{}.set_fg(default_color(DefaultColor::Muted)));
    layout::draw_axis_x(sink, area_, xticks, "x", Style{});
    layout::draw_axis_y(sink, area_, yticks, "F(x)", Style{});

    const Style st = Style{}.set_fg(color_);
    const Rect inner = area_.inner_rect();
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);

    double prev_x = inner.x;
    double prev_y = area_.y_scale().forward(0.0);
    std::size_t i = 0;
    while (i < n) {
        std::size_t j = i;
        while (j < n && sorted_[j] == sorted_[i]) {
            ++j;
        }
        const double level = static_cast<double>(j) / static_cast<double>(n);
        const double x = area_.x_scale().forward(sorted_[i]);
        const double y = area_.y_scale().forward(level);
        if (x > prev_x) {
            b.stroke(st.fg());
            b.move_to(Point{prev_x, prev_y});
            b.line_to(Point{x, prev_y});
            if (options_.show_risers) {
                b.stroke(st.fg());
                b.move_to(Point{x, prev_y});
                b.line_to(Point{x, y});
            }
        }
        if (options_.show_markers) {
            sink.put(Point{x, y}, kMarkerChar, st);
        }
        prev_x = x;
        prev_y = y;
        i = j;
    }
    if (prev_x < inner.right()) {
        b.stroke(st.fg());
        b.move_to(Point{prev_x, prev_y});
        b.line_to(Point{inner.right(), prev_y});
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
