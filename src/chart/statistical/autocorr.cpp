// autocorr.cpp —— ACF 实现：滞后乘积和 + 棒棒糖落格
#include "termchart/chart/statistical/autocorr.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "termchart/chart/scene_sink.hpp"

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {

void AutocorrChart::set_values(std::vector<double> values, Color color) {
    values_.clear();
    values_.reserve(values.size());
    for (double x : values) {
        if (std::isfinite(x)) {
            values_.push_back(x);
        }
    }
    color_ = color;

    acf_.clear();
    conf_ = 0.0;
    const std::size_t n = values_.size();
    if (n < 2) {
        return;
    }
    double mean = 0.0;
    for (double x : values_) {
        mean += x;
    }
    mean /= static_cast<double>(n);
    double denom = 0.0;
    for (double x : values_) {
        const double d = x - mean;
        denom += d * d;
    }
    if (!(denom > 0.0)) {
        return;  // 常数序列：自相关无定义
    }
    const auto max_lag = static_cast<std::size_t>(
        std::clamp(options_.max_lag, 1, static_cast<int>(n) - 1));
    acf_.assign(max_lag + 1, 0.0);
    acf_[0] = 1.0;
    for (std::size_t k = 1; k <= max_lag; ++k) {
        double acc = 0.0;
        for (std::size_t i = k; i < n; ++i) {
            acc += (values_[i] - mean) * (values_[i - k] - mean);
        }
        acf_[k] = acc / denom;
    }
    conf_ = 1.96 / std::sqrt(static_cast<double>(n));
}

void AutocorrChart::draw(DrawContext ctx) {
    if (acf_.size() < 2) {
        return;  // 只有 r[0]（或空）：没有可画的滞后
    }
    double lo = -conf_;
    double hi = conf_;
    for (std::size_t k = 1; k < acf_.size(); ++k) {
        lo = std::min(lo, acf_[k]);
        hi = std::max(hi, acf_[k]);
    }
    if (!(hi > lo)) {
        lo -= 0.5;
        hi += 0.5;
    }
    const double xmax = static_cast<double>(acf_.size() - 1) + 0.5;
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, 0.5, xmax, lo, hi, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(default_color(DefaultColor::Muted))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "lag", ctx.axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "r", ctx.axis_style);
    }

    const Style st = Style{}.set_fg(color_);
    const Rect inner = area_.inner_rect();
    const double y0 = area_.y_scale().forward(0.0);
    if (options_.show_band && conf_ > 0.0) {
        const Style band = Style{}.set_fg(default_color(DefaultColor::Muted));
        for (double level : {-conf_, conf_}) {
            const double y = area_.y_scale().forward(level);
            ctx.canvas.draw_line(Point{inner.x, y}, Point{inner.right(), y},
                                 U'\u00B7', band);
        }
    }
    for (std::size_t k = 1; k < acf_.size(); ++k) {
        const double x = area_.x_scale().forward(static_cast<double>(k));
        const double y = area_.y_scale().forward(acf_[k]);
        ctx.canvas.draw_line(Point{x, y0}, Point{x, y}, U'\u2502', st);
        ctx.canvas.put(Point{x, y}, kMarkerChar, st);
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

bool AutocorrChart::to_scene(scene::Scene& out, std::uint32_t width,
                             std::uint32_t height) const {
    if (acf_.size() < 2) {
        return true;  // 只有 r[0]（或空）：没有可画的滞后
    }
    double lo = -conf_;
    double hi = conf_;
    for (std::size_t k = 1; k < acf_.size(); ++k) {
        lo = std::min(lo, acf_[k]);
        hi = std::max(hi, acf_[k]);
    }
    if (!(hi > lo)) {
        lo -= 0.5;
        hi += 0.5;
    }
    const double xmax = static_cast<double>(acf_.size() - 1) + 0.5;
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(width, height, m, 0.5, xmax, lo, hi, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    SceneSink sink(out);
    layout::draw_grid(sink, area_, xticks, yticks,
                      Style{}.set_fg(default_color(DefaultColor::Muted)));
    layout::draw_axis_x(sink, area_, xticks, "lag", Style{});
    layout::draw_axis_y(sink, area_, yticks, "r", Style{});

    const Style st = Style{}.set_fg(color_);
    const Rect inner = area_.inner_rect();
    const double y0 = area_.y_scale().forward(0.0);
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);

    // ±1.96/√n 置信带：水平参考线（矢量）。
    if (options_.show_band && conf_ > 0.0) {
        const double y1 = area_.y_scale().forward(conf_);
        const double y2 = area_.y_scale().forward(-conf_);
        b.stroke(default_color(DefaultColor::Muted));
        b.move_to(Point{inner.x, y1});
        b.line_to(Point{inner.right(), y1});
        b.stroke(default_color(DefaultColor::Muted));
        b.move_to(Point{inner.x, y2});
        b.line_to(Point{inner.right(), y2});
    }

    // 棒棒糖：零线到 r_k 的竖线 + 顶端 '●'。
    for (std::size_t k = 1; k < acf_.size(); ++k) {
        const double x = area_.x_scale().forward(static_cast<double>(k));
        const double y = area_.y_scale().forward(acf_[k]);
        b.stroke(st.fg());
        b.move_to(Point{x, y0});
        b.line_to(Point{x, y});
        sink.put(Point{x, y}, kMarkerChar, st);
    }
    b.end_group(0);
    if (!title_.empty()) {
        layout::draw_title(sink, title_, inner);
    }
    return true;
}

}  // namespace termchart::chart
