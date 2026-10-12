// ridgeline.cpp —— 山脊图实现（全局分箱 → 逐组直方 → 行间重叠填充）
#include "termchart/chart/cartesian/ridgeline.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "termchart/canvas/canvas.hpp"
#include "termchart/detail/block_chars.hpp"
#include "termchart/layout/axis.hpp"
#include "termchart/layout/legend.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {

void RidgelineChart::set_groups(std::vector<Group> groups) { groups_ = std::move(groups); }

void RidgelineChart::draw(DrawContext ctx) {
    // 逐组保留有限值；没有有效值的组直接跳过。
    std::vector<Group> groups;
    groups.reserve(groups_.size());
    for (Group& g : groups_) {
        Group clean;
        clean.name = std::move(g.name);
        clean.color = g.color;
        for (const double v : g.values) {
            if (std::isfinite(v)) {
                clean.values.push_back(v);
            }
        }
        if (!clean.values.empty()) {
            groups.push_back(std::move(clean));
        }
    }
    if (groups.empty()) {
        return;
    }

    // 全局值域：所有组共用 x 轴，山脊才有可比性。
    double vmin = groups.front().values.front();
    double vmax = groups.front().values.front();
    for (const Group& g : groups) {
        for (const double v : g.values) {
            vmin = std::min(vmin, v);
            vmax = std::max(vmax, v);
        }
    }
    if (!(vmax > vmin)) {
        vmin -= 0.5;
        vmax += 0.5;
    }

    const int bins = std::clamp(options_.bins, 4, 128);
    const auto dim = static_cast<std::size_t>(bins);
    const double bin_w = (vmax - vmin) / static_cast<double>(bins);

    // 分箱计数：全局找最大计数做归一化基准。
    std::vector<std::size_t> maxima(groups.size(), 0);
    std::vector<std::vector<std::size_t>> counts(groups.size());
    std::size_t global_max = 0;
    for (std::size_t g = 0; g < groups.size(); ++g) {
        counts[g].assign(dim, 0);
        for (const double v : groups[g].values) {
            int b = static_cast<int>(std::floor((v - vmin) / bin_w));
            b = std::clamp(b, 0, bins - 1);
            ++counts[g][static_cast<std::size_t>(b)];
        }
        for (const std::size_t c : counts[g]) {
            maxima[g] = std::max(maxima[g], c);
            global_max = std::max(global_max, c);
        }
    }
    if (global_max == 0) {
        return;  // 理论上不可达（组非空），保守兜底
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, vmin, vmax, 0.0, 1.0, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "value", ctx.axis_style);
    }

    const Rect inner = area_.inner_rect();
    const auto inner_h = static_cast<int>(inner.h);
    const int g = static_cast<int>(groups.size());
    // 行间重叠一格：若总高超过内框则压缩每组行数，保底 2 行。
    int band = std::max(options_.band_height, 2);
    while (band + (g - 1) * (band - 1) > inner_h && band > 2) {
        --band;
    }
    const int offset = band - 1;

    // 从最后一组（最上、最远）画到第一组（最下、最近），近处覆盖远处形成遮挡。
    for (int gi = g - 1; gi >= 0; --gi) {
        const Group& grp = groups[static_cast<std::size_t>(gi)];
        const double y_base = inner.bottom() - 1.0 -
                              static_cast<double>(gi) * static_cast<double>(offset);
        const Style st = Style{}.set_fg(grp.color);
        for (int b = 0; b < bins; ++b) {
            const std::size_t c = counts[static_cast<std::size_t>(gi)][static_cast<std::size_t>(b)];
            if (c == 0) {
                continue;
            }
            const double t = static_cast<double>(c) / static_cast<double>(global_max);
            const double h = t * static_cast<double>(band);
            const int full = static_cast<int>(h);
            const double frac = h - static_cast<double>(full);
            const double x0 = area_.x_scale().forward(vmin + bin_w * static_cast<double>(b));
            const double x1 = area_.x_scale().forward(vmin + bin_w * static_cast<double>(b + 1));
            const double y_top = y_base - static_cast<double>(full) + 1.0;
            const Rect body{std::min(x0, x1), y_top, std::abs(x1 - x0),
                            static_cast<double>(full)};
            const Rect clipped = body.intersection(inner);
            if (full > 0 && !clipped.empty()) {
                ctx.canvas.fill_rect(clipped, detail::kFullBlock, st);
            }
            // 分数行：用下八分块表达不足一格的密度。
            if (frac > 0.125) {
                const double fy = y_top - 1.0;
                if (fy >= inner.y) {
                    const int eighths = std::clamp(
                        static_cast<int>(frac * 8.0 + 0.5), 1, 8);
                    ctx.canvas.fill_rect(Rect{x0, fy, x1 - x0, 1.0},
                                         detail::lower_eighth(eighths), st);
                }
            }
        }
    }

    if (options_.show_legend && ctx.draw_legend) {
        std::vector<layout::LegendEntry> entries;
        entries.reserve(groups.size());
        for (const Group& grp : groups) {
            entries.push_back(layout::LegendEntry{grp.name, grp.color});
        }
        layout::draw_legend(ctx.canvas, entries, Rect{inner.x, 0.0, inner.w, 1.0},
                            layout::HAlign::Left);
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

}  // namespace termchart::chart
