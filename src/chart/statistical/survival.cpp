// survival.cpp —— Kaplan-Meier 实现：事件时刻乘积估计 + 删失刻度
#include "termchart/chart/statistical/survival.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/legend.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {

void SurvivalChart::set_data(std::vector<double> times, std::vector<int> events,
                             Color color, std::string name) {
    options_.color = color;
    name_ = std::move(name);
    curve_.clear();
    censored_.clear();

    const std::size_t n = std::min(times.size(), events.size());
    std::vector<std::pair<double, int>> obs;
    obs.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (std::isfinite(times[i])) {
            obs.emplace_back(times[i], events[i]);
        }
    }
    if (obs.empty()) {
        return;
    }
    std::sort(obs.begin(), obs.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    // KM：同刻合并；n_i = time ≥ t_i 的个体数（含删失）。
    double s = 1.0;
    const double total = static_cast<double>(obs.size());
    std::size_t i = 0;
    while (i < obs.size()) {
        const double t = obs[i].first;
        long events_at = 0;
        std::size_t j = i;
        while (j < obs.size() && obs[j].first == t) {
            if (obs[j].second != 0) {
                ++events_at;
            } else {
                censored_.push_back(t);
            }
            ++j;
        }
        if (events_at > 0) {
            const double at_risk = total - static_cast<double>(i);
            s *= 1.0 - static_cast<double>(events_at) / at_risk;
            curve_.push_back(Point{t, s});
        }
        i = j;
    }
}

void SurvivalChart::draw(DrawContext ctx) {
    if (curve_.empty()) {
        return;
    }
    // x 域覆盖到最后一个删失时刻：删失可以发生在末次事件之后。
    double tmax = curve_.back().x;
    if (!censored_.empty()) {
        tmax = std::max(tmax, censored_.back());
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, 0.0, tmax > 0.0 ? tmax : 1.0, 0.0, 1.0,
                      false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(Color::ansi16(8))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "time", ctx.axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "S(t)", ctx.axis_style);
    }

    const Style st = Style{}.set_fg(options_.color);
    const Rect inner = area_.inner_rect();
    // 起点：S = 1 从 t = 0 起水平延伸到首个事件时刻。
    double prev_x = inner.x;
    double prev_y = area_.y_scale().forward(1.0);
    std::size_t cens = 0;  // 删失时刻与曲线同升序，双指针归并
    for (const Point& step : curve_) {
        const double x = area_.x_scale().forward(step.x);
        const double y = area_.y_scale().forward(step.y);
        if (x > prev_x) {
            ctx.canvas.draw_line(Point{prev_x, prev_y}, Point{x, prev_y},
                                 U'\u2500', st);
            ctx.canvas.draw_line(Point{x, prev_y}, Point{x, y}, U'\u2502', st);
        }
        while (cens < censored_.size() && censored_[cens] <= step.x) {
            if (options_.show_censored) {
                const double cx = area_.x_scale().forward(censored_[cens]);
                ctx.canvas.put(Point{cx, prev_y}, U'+', st);
            }
            ++cens;
        }
        prev_x = x;
        prev_y = y;
    }
    // 末次事件之后仍存在的删失刻度补画（此刻 S 即当前水平）。
    while (cens < censored_.size()) {
        if (options_.show_censored) {
            const double cx = area_.x_scale().forward(censored_[cens]);
            ctx.canvas.put(Point{cx, prev_y}, U'+', st);
        }
        ++cens;
    }
    if (prev_x < inner.right()) {
        ctx.canvas.draw_line(Point{prev_x, prev_y}, Point{inner.right(), prev_y},
                             U'\u2500', st);
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

}  // namespace termchart::chart
