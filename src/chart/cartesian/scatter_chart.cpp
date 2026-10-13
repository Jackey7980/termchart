// scatter_chart.cpp —— 散点图实现
#include "termchart/chart/cartesian/scatter_chart.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

// 四档气泡标记：权重归一化后按 25% 分档，避免"半个点的半径"这种画不出来的东西。
constexpr char32_t kBubbleDots[4] = {U'\u00B7', U'\u25CB', U'\u25CF', U'\u25C9'};

char32_t bubble_marker(double weight, double max_weight) noexcept {
    if (max_weight <= 0.0) {
        return kBubbleDots[1];
    }
    const double ratio = std::clamp(weight / max_weight, 0.0, 1.0);
    if (ratio < 0.25) {
        return kBubbleDots[0];
    }
    if (ratio < 0.5) {
        return kBubbleDots[1];
    }
    if (ratio < 0.75) {
        return kBubbleDots[2];
    }
    return kBubbleDots[3];
}

}  // namespace

void ScatterChart::add_series(Series series) {
    series_.push_back(std::move(series));
    weights_.emplace_back();
}

void ScatterChart::set_series(std::vector<Series> series) {
    series_ = std::move(series);
    weights_.assign(series_.size(), {});  // 权重按下标一一对应，必须整重置
}

void ScatterChart::add_bubble_series(Series series, std::vector<double> weights) {
    series_.push_back(std::move(series));
    weights_.push_back(std::move(weights));
}

void ScatterChart::draw(DrawContext ctx) {
    double xmin = 0.0;
    double xmax = 0.0;
    double ymin = 0.0;
    double ymax = 0.0;
    data_bounds(series_, xmin, xmax, ymin, ymax);
    if (!std::isfinite(xmin)) {
        xmin = 0.0;
        xmax = 1.0;
        ymin = 0.0;
        ymax = 1.0;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, xmin, xmax, ymin, ymax, auto_domain_, include_zero_);

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

    for (std::size_t s = 0; s < series_.size(); ++s) {
        const Series& series = series_[s];
        const Style style = Style{}.set_fg(series.color);
        const std::vector<double>* weights =
            s < weights_.size() && !weights_[s].empty() ? &weights_[s] : nullptr;
        double max_weight = 0.0;
        if (weights != nullptr) {
            for (const double w : *weights) {
                max_weight = std::max(max_weight, w);
            }
        }
        for (std::size_t i = 0; i < series.points.size(); ++i) {
            const Point cell = area_.cell_of(series.points[i].x, series.points[i].y);
            if (weights == nullptr) {
                ctx.canvas.put(cell, kMarkerChar, style);
                continue;
            }
            const double weight = i < weights->size() ? (*weights)[i] : 0.0;
            ctx.canvas.put(cell, bubble_marker(weight, max_weight), style);
        }
    }

    draw_legend_for(ctx.canvas, series_, area_, show_legend_ && ctx.draw_legend);
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, area_.inner_rect());
    }
}

}  // namespace termchart::chart
