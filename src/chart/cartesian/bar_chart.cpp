// bar_chart.cpp —— 笛卡尔柱状图实现（单系列，垂直柱）
#include "termchart/chart/cartesian/bar_chart.hpp"

#include <cmath>

#include "termchart/chart/scene_sink.hpp"
#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/legend.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {

void BarChart::set_values(std::vector<double> values,
                          std::vector<std::string> categories, Color color,
                          std::string name) {
    values_ = std::move(values);
    categories_ = std::move(categories);
    color_ = color;
    name_ = std::move(name);
}

void BarChart::draw(DrawContext ctx) {
    if (values_.empty()) {
        return;  // 无数据：不绘制，避免内框按列数划分时除零
    }
    double ymin = 0.0;
    double ymax = 0.0;
    for (double v : values_) {
        ymax = std::max(ymax, v);
        ymin = std::min(ymin, v);
    }
    const double xmin = 0.0;
    const double xmax = static_cast<double>(values_.size());
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, xmin, xmax, ymin, ymax, true, false, y_domain_);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());

    // 上下文开关（默认全开）：柱状图同样允许"只要数据不要轴"。
    const Style axis_style = ctx.axis_style;
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "category", axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "value", axis_style);
    }

    const Style st = Style{}.set_fg(color_);
    const Rect inner = area_.inner_rect();
    const double slot = inner.w / static_cast<double>(values_.size());
    const double bw = slot * 0.7;
    // 动画进度按系列名查：没登记 → 1.0（原样），登记过则柱子从**基线 0** 长到目标值。
    // 注意生长的是"值"，不是"高度"——负值柱因此向下长，和正的对称。
    const double progress = progress_of(name_);
    if (progress > 0.0) {
        for (std::size_t i = 0; i < values_.size(); ++i) {
            const double x0 = area_.x_scale().forward(static_cast<double>(i) + 0.5);
            const double y0 = area_.y_scale().forward(0.0);
            const double y1 = area_.y_scale().forward(values_[i] * progress);
            const double bx = x0 - bw / 2.0;
            ctx.canvas.fill_rect(Rect{bx, std::min(y0, y1), bw, std::fabs(y1 - y0)},
                                 kBarChar, st);
        }
    }

    if (show_legend_ && ctx.draw_legend) {
        std::vector<layout::LegendEntry> entries{layout::LegendEntry{name_, color_}};
        layout::draw_legend(ctx.canvas, entries, Rect{inner.x, 0.0, inner.w, 1.0},
                            layout::HAlign::Left);
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

bool BarChart::to_scene(scene::Scene& out, std::uint32_t width, std::uint32_t height) const {
    if (values_.empty()) {
        return true;  // 空图合法：一个指令都不发，调用方仍能拿到可导出的空 Scene
    }
    double ymin = 0.0;
    double ymax = 0.0;
    for (double v : values_) {
        ymax = std::max(ymax, v);
        ymin = std::min(ymin, v);
    }
    const double xmin = 0.0;
    const double xmax = static_cast<double>(values_.size());
    // 与 draw() 同一个 make_area：布局只有一份。
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(width, height, m, xmin, xmax, ymin, ymax, true, false, y_domain_);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());

    SceneSink sink(out);
    layout::draw_grid(sink, area_, xticks, yticks,
                      Style{}.set_fg(default_color(DefaultColor::Muted)));
    layout::draw_axis_x(sink, area_, xticks, "category", Style{});
    layout::draw_axis_y(sink, area_, yticks, "value", Style{});

    scene::SceneBuilder& builder = sink.builder();
    const Rect inner = area_.inner_rect();
    const double slot = inner.w / static_cast<double>(values_.size());
    const double bw = slot * 0.7;

    builder.begin_group(0);  // 整组柱子一个分组：交互层按组命中即"这条系列"
    builder.fill(color_);
    // 完整几何（不看进度）：柱高生长由 `apply_animation` 在 Scene 层做，
    // 它按"基线侧固定、另一侧按进度缩"改写 RectOp——正负柱都成立。
    for (std::size_t i = 0; i < values_.size(); ++i) {
        const double x0 = area_.x_scale().forward(static_cast<double>(i) + 0.5);
        const double y0 = area_.y_scale().forward(0.0);
        const double y1 = area_.y_scale().forward(values_[i]);
        const double bx = x0 - bw / 2.0;
        builder.rect(Rect{bx, std::min(y0, y1), bw, std::fabs(y1 - y0)});
    }
    builder.end_group(0);

    if (show_legend_) {
        std::vector<layout::LegendEntry> entries{layout::LegendEntry{name_, color_}};
        layout::draw_legend(sink, entries, Rect{inner.x, 0.0, inner.w, 1.0}, layout::HAlign::Left);
    }
    if (!title_.empty()) {
        layout::draw_title(sink, title_, inner);
    }
    return true;
}

void BarChart::animation_groups(std::vector<GroupProgress>& out) const {
    if (values_.empty()) {
        return;  // to_scene 对空数据不产出组，这里也不能凭空造一个
    }
    out.push_back(GroupProgress{0, progress_of(name_)});
}

}  // namespace termchart::chart
