// violin.cpp —— 小提琴图实现（高斯核密度 → 对称条带宽度）
#include "termchart/chart/cartesian/violin.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/chart/scene_sink.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

inline constexpr char32_t kViolinChar = U'\u2588';  // █ 密度条带
inline constexpr char32_t kMedianMark = U'\u2500';  // ─ 中位数

// Silverman 经验带宽：h = 1.06 * σ * n^(-1/5)；σ 退化（单点/全同值）时取 1。
double silverman_bandwidth(const std::vector<double>& xs) noexcept {
    const double n = static_cast<double>(xs.size());
    if (xs.size() < 2) {
        return 1.0;
    }
    double mean = 0.0;
    for (double v : xs) {
        mean += v;
    }
    mean /= n;
    double var = 0.0;
    for (double v : xs) {
        var += (v - mean) * (v - mean);
    }
    var /= (n - 1.0);
    const double sigma = std::sqrt(var);
    if (!(sigma > 0.0)) {
        return 1.0;
    }
    return 1.06 * sigma * std::pow(n, -0.2);
}

}  // namespace

void ViolinChart::draw(DrawContext ctx) {
    // 每组只留有限样本；空组整体跳过。
    std::vector<std::vector<double>> groups;
    groups.reserve(groups_.size());
    for (const std::vector<double>& g : groups_) {
        std::vector<double> xs;
        xs.reserve(g.size());
        for (double v : g) {
            if (std::isfinite(v)) {
                xs.push_back(v);
            }
        }
        if (!xs.empty()) {
            groups.push_back(std::move(xs));
        }
    }
    if (groups.empty()) {
        return;
    }

    double ymin = groups.front().front();
    double ymax = ymin;
    for (const std::vector<double>& xs : groups) {
        ymin = std::min(ymin, xs.front());
        ymax = std::max(ymax, xs.front());
        for (double v : xs) {
            ymin = std::min(ymin, v);
            ymax = std::max(ymax, v);
        }
    }
    if (!(ymax > ymin)) {
        ymax = ymin + 1.0;  // 全同值：给单位高度避免零宽域
    }

    const auto n = static_cast<double>(groups.size());
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, -0.5, n - 0.5, ymin, ymax, false, false);

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
        layout::draw_axis_x(ctx.canvas, area_, xticks, "group", axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "value", axis_style);
    }

    const Rect inner = area_.inner_rect();
    const int bands = opts_.bands > 0 ? std::clamp(opts_.bands, 4, 128)
                                      : std::clamp(static_cast<int>(inner.h), 4, 64);
    const Style st = Style{}.set_fg(opts_.color);
    // 每组的最大半宽（格）：占格宽的一半再留 1 格组间缝。
    const double half_w = std::max(0.5, inner.w / n * 0.5 - 0.5);

    for (std::size_t g = 0; g < groups.size(); ++g) {
        const std::vector<double>& xs = groups[g];
        const double xc = area_.x_scale().forward(static_cast<double>(g));
        const double h = silverman_bandwidth(xs);

        std::vector<double> density(static_cast<std::size_t>(bands), 0.0);
        double dmax = 0.0;
        for (int k = 0; k < bands; ++k) {
            const double t =
                ymin + (static_cast<double>(k) + 0.5) *
                           (ymax - ymin) / static_cast<double>(bands);
            double d = 0.0;
            for (double v : xs) {
                const double u = (t - v) / h;
                d += std::exp(-0.5 * u * u);
            }
            density[static_cast<std::size_t>(k)] = d;
            dmax = std::max(dmax, d);
        }
        if (!(dmax > 0.0)) {
            continue;
        }

        for (int k = 0; k < bands; ++k) {
            const double t =
                ymin + (static_cast<double>(k) + 0.5) *
                           (ymax - ymin) / static_cast<double>(bands);
            // 密度 → 对称条带半宽；密度 >0 时至少 1 格宽。
            const double t_norm = density[static_cast<std::size_t>(k)] / dmax;
            const double hw = std::max(0.5, t_norm * half_w);
            const double row = area_.y_scale().forward(t);
            ctx.canvas.fill_rect(Rect{xc - hw, row, hw * 2.0, 1.0}, kViolinChar, st);
        }

        if (opts_.show_median) {
            // 中位数：排序后取中（副本排序，不动调用方数据）。
            std::vector<double> sorted = xs;
            std::sort(sorted.begin(), sorted.end());
            const double med = sorted.size() % 2 == 1
                                   ? sorted[sorted.size() / 2]
                                   : (sorted[sorted.size() / 2 - 1] +
                                      sorted[sorted.size() / 2]) / 2.0;
            ctx.canvas.put(Point{xc, area_.y_scale().forward(med)}, kMedianMark, st);
        }
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

bool ViolinChart::to_scene(scene::Scene& out, std::uint32_t width, std::uint32_t height) const {
    std::vector<std::vector<double>> groups;
    groups.reserve(groups_.size());
    for (const std::vector<double>& g : groups_) {
        std::vector<double> xs;
        xs.reserve(g.size());
        for (double v : g) {
            if (std::isfinite(v)) {
                xs.push_back(v);
            }
        }
        if (!xs.empty()) {
            groups.push_back(std::move(xs));
        }
    }
    if (groups.empty()) {
        return true;
    }

    double ymin = groups.front().front();
    double ymax = ymin;
    for (const auto& xs : groups) {
        for (double v : xs) {
            ymin = std::min(ymin, v);
            ymax = std::max(ymax, v);
        }
    }
    if (!(ymax > ymin)) {
        ymax = ymin + 1.0;
    }

    const auto n = static_cast<double>(groups.size());
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(width, height, m, -0.5, n - 0.5, ymin, ymax, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    SceneSink sink(out);
    layout::draw_grid(sink, area_, xticks, yticks, Style{}.set_fg(default_color(DefaultColor::Muted)));
    layout::draw_axis_x(sink, area_, xticks, "group", Style{});
    layout::draw_axis_y(sink, area_, yticks, "value", Style{});

    const Rect inner = area_.inner_rect();
    const int bands = opts_.bands > 0 ? std::clamp(opts_.bands, 4, 128)
                                      : std::clamp(static_cast<int>(inner.h), 4, 64);
    const Style st = Style{}.set_fg(opts_.color);
    const double half_w = std::max(0.5, inner.w / n * 0.5 - 0.5);

    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);
    for (std::size_t g = 0; g < groups.size(); ++g) {
        const std::vector<double>& xs = groups[g];
        const double xc = area_.x_scale().forward(static_cast<double>(g));
        const double h = silverman_bandwidth(xs);

        std::vector<double> density(static_cast<std::size_t>(bands), 0.0);
        double dmax = 0.0;
        for (int k = 0; k < bands; ++k) {
            const double t =
                ymin + (static_cast<double>(k) + 0.5) * (ymax - ymin) /
                           static_cast<double>(bands);
            double d = 0.0;
            for (double v : xs) {
                const double u = (t - v) / h;
                d += std::exp(-0.5 * u * u);
            }
            density[static_cast<std::size_t>(k)] = d;
            dmax = std::max(dmax, d);
        }
        if (!(dmax > 0.0)) {
            continue;
        }

        for (int k = 0; k < bands; ++k) {
            const double t =
                ymin + (static_cast<double>(k) + 0.5) * (ymax - ymin) /
                           static_cast<double>(bands);
            const double t_norm = density[static_cast<std::size_t>(k)] / dmax;
            const double hw = std::max(0.5, t_norm * half_w);
            const double row = area_.y_scale().forward(t);
            b.fill(st.fg());
            b.rect(Rect{xc - hw, row, hw * 2.0, 1.0});
        }
        if (opts_.show_median) {
            std::vector<double> sorted = xs;
            std::sort(sorted.begin(), sorted.end());
            const double med = sorted.size() % 2 == 1
                                   ? sorted[sorted.size() / 2]
                                   : (sorted[sorted.size() / 2 - 1] +
                                      sorted[sorted.size() / 2]) / 2.0;
            sink.put(Point{xc, area_.y_scale().forward(med)}, kMedianMark, st);
        }
    }
    b.end_group(0);

    if (!title_.empty()) {
        layout::draw_title(sink, title_, inner);
    }
    return true;
}

}  // namespace termchart::chart
