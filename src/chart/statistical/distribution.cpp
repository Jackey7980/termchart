// distribution.cpp —— 分布曲线实现：Silverman 带宽 + 高斯核 KDE
#include "termchart/chart/statistical/distribution.hpp"

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
namespace {

constexpr double kInvSqrtTwoPi = 0.39894228040143267794;

// 线性插值分位数（v 升序，q ∈ [0,1]）。
double quantile(const std::vector<double>& v, double q) noexcept {
    const double pos = q * static_cast<double>(v.size() - 1);
    const auto lo = static_cast<std::size_t>(pos);
    const auto hi = lo + 1 < v.size() ? lo + 1 : lo;
    const double frac = pos - static_cast<double>(lo);
    return v[lo] * (1.0 - frac) + v[hi] * frac;
}

}  // namespace

void DistributionChart::set_values(std::vector<double> values, Color color,
                                   std::string name) {
    values_ = std::move(values);
    color_ = color;
    name_ = std::move(name);
    evaluate();
}

void DistributionChart::evaluate() {
    xs_.clear();
    ys_.clear();
    std::vector<double> v;
    v.reserve(values_.size());
    for (double x : values_) {
        if (std::isfinite(x)) {
            v.push_back(x);
        }
    }
    if (v.empty()) {
        return;
    }
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();

    double mean = 0.0;
    for (double x : v) {
        mean += x;
    }
    mean /= static_cast<double>(n);
    double var = 0.0;
    for (double x : v) {
        const double d = x - mean;
        var += d * d;
    }
    var /= static_cast<double>(n);

    double h = options_.bandwidth;
    if (!(h > 0.0)) {
        // Silverman：h = 0.9·min(σ, IQR/1.34)·n^(-1/5)，尺度退化取单位带宽。
        const double sd = std::sqrt(var > 0.0 ? var : 0.0);
        const double iqr = quantile(v, 0.75) - quantile(v, 0.25);
        double s = std::min(sd, iqr > 0.0 ? iqr / 1.34 : sd);
        if (!(s > 0.0)) {
            s = 1.0;
        }
        h = 0.9 * s * std::pow(static_cast<double>(n), -0.2);
        if (!(h > 0.0) || !std::isfinite(h)) {
            h = 1.0;
        }
    }

    const double lo = v.front() - 3.0 * h;
    const double hi = v.back() + 3.0 * h;
    const auto gp = static_cast<std::size_t>(
        std::clamp(options_.grid_points, 16, 512));
    const double norm = kInvSqrtTwoPi / (static_cast<double>(n) * h);
    xs_.resize(gp);
    ys_.resize(gp);
    for (std::size_t i = 0; i < gp; ++i) {
        const double x =
            lo + (hi - lo) * static_cast<double>(i) / static_cast<double>(gp - 1);
        double density = 0.0;
        for (double xi : v) {
            const double u = (x - xi) / h;
            density += std::exp(-0.5 * u * u);
        }
        xs_[i] = x;
        ys_[i] = density * norm;
    }
}

void DistributionChart::draw(DrawContext ctx) {
    if (xs_.empty() || xs_.size() != ys_.size()) {
        return;
    }
    double ymax = 0.0;
    for (double y : ys_) {
        ymax = std::max(ymax, y);
    }
    if (!(ymax > 0.0)) {
        return;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, xs_.front(), xs_.back(), 0.0, ymax, false, false);

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
        layout::draw_axis_y(ctx.canvas, area_, yticks, "density", ctx.axis_style);
    }

    const Style st = Style{}.set_fg(color_);
    const Rect inner = area_.inner_rect();
    for (std::size_t i = 0; i < xs_.size(); ++i) {
        const Point p = area_.cell_of(xs_[i], ys_[i]);
        if (options_.fill_under) {
            // 曲线下方铺浅色：从曲线格到绘图区底边（列粒度）。
            ctx.canvas.fill_rect(Rect{p.x, p.y, 1.0, inner.bottom() - p.y},
                                 U'\u2592', st);
        }
        ctx.canvas.put(p, kLineChar, st);
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

bool DistributionChart::to_scene(scene::Scene& out, std::uint32_t width,
                                  std::uint32_t height) const {
    if (xs_.empty() || xs_.size() != ys_.size()) {
        return true;
    }
    double ymax = 0.0;
    for (double y : ys_) {
        ymax = std::max(ymax, y);
    }
    if (!(ymax > 0.0)) {
        return true;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(width, height, m, xs_.front(), xs_.back(), 0.0, ymax, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    SceneSink sink(out);
    layout::draw_grid(sink, area_, xticks, yticks,
                      Style{}.set_fg(default_color(DefaultColor::Muted)));
    layout::draw_axis_x(sink, area_, xticks, "x", Style{});
    layout::draw_axis_y(sink, area_, yticks, "density", Style{});

    const Style st = Style{}.set_fg(color_);
    const Rect inner = area_.inner_rect();
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);
    for (std::size_t i = 0; i < xs_.size(); ++i) {
        const Point p = area_.cell_of(xs_[i], ys_[i]);
        if (options_.fill_under) {
            b.fill(st.fg());
            b.rect(Rect{p.x, p.y, 1.0, inner.bottom() - p.y});
        }
        sink.put(p, kLineChar, st);
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
