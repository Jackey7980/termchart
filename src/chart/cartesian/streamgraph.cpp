// streamgraph.cpp —— 流图实现（逐列堆叠 + 居中/最小摆动基线）
#include "termchart/chart/cartesian/streamgraph.hpp"

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

void StreamgraphChart::add_series(Series series) { layers_.push_back(std::move(series)); }

void StreamgraphChart::draw(DrawContext ctx) {
    // 只保留非空层；各层长度以最短者为准（按序号堆叠，超出部分丢弃）。
    std::vector<const Series*> layers;
    layers.reserve(layers_.size());
    for (const Series& s : layers_) {
        if (!s.points.empty()) {
            layers.push_back(&s);
        }
    }
    if (layers.empty()) {
        return;
    }
    std::size_t n = layers.front()->points.size();
    for (const Series* s : layers) {
        n = std::min(n, s->points.size());
    }

    // 逐列堆叠：先取值（非有限按 0 处理），再按基线算法求每层上下边界。
    std::vector<double> top(n * layers.size(), 0.0);
    std::vector<double> bot(n * layers.size(), 0.0);
    double ymin = 0.0;
    double ymax = 0.0;
    for (std::size_t j = 0; j < n; ++j) {
        std::vector<double> v(layers.size(), 0.0);
        double total = 0.0;
        for (std::size_t i = 0; i < layers.size(); ++i) {
            const double y = layers[i]->points[j].y;
            v[i] = std::isfinite(y) ? y : 0.0;
            total += v[i];
        }
        double base = 0.0;
        if (options_.baseline == Baseline::Silhouette) {
            base = -total / 2.0;
        } else {  // Wiggle：按层位加权偏移，最小化各层摆动
            const double g = static_cast<double>(layers.size());
            for (std::size_t i = 0; i < layers.size(); ++i) {
                base -= v[i] * (g - static_cast<double>(i) - (g + 1.0) / 2.0) / g;
            }
        }
        double acc = base;
        for (std::size_t i = 0; i < layers.size(); ++i) {
            bot[j * layers.size() + i] = acc;
            acc += v[i];
            top[j * layers.size() + i] = acc;
            ymin = std::min(ymin, bot[j * layers.size() + i]);
            ymax = std::max(ymax, top[j * layers.size() + i]);
        }
    }

    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    // x 轴是观测序号：n==1 时给单位跨度避免零宽定义域。
    const double xmax = n > 1 ? static_cast<double>(n - 1) : 1.0;
    area_ = make_area(ctx.canvas, m, 0.0, xmax, ymin, ymax, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    if (ctx.draw_axes) {
        layout::draw_axis_x(ctx.canvas, area_, xticks, "index", ctx.axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "", ctx.axis_style);
    }

    const Rect inner = area_.inner_rect();
    for (std::size_t j = 0; j < n; ++j) {
        // 列的格范围：本列序号 → 下一列序号；最后一列贴到内框右缘。
        const double x0 = area_.x_scale().forward(static_cast<double>(j));
        const double x1 = j + 1 < n
                              ? area_.x_scale().forward(static_cast<double>(j + 1))
                              : inner.right();
        for (std::size_t i = 0; i < layers.size(); ++i) {
            const double y0 = area_.y_scale().forward(top[j * layers.size() + i]);
            const double y1 = area_.y_scale().forward(bot[j * layers.size() + i]);
            const Rect seg = Rect{std::min(x0, x1), std::min(y0, y1), std::abs(x1 - x0),
                                  std::abs(y1 - y0)}.intersection(inner);
            if (seg.empty()) {
                continue;
            }
            ctx.canvas.fill_rect(seg, detail::kFullBlock,
                                 Style{}.set_fg(layers[i]->color));
        }
    }

    if (ctx.draw_legend) {
        std::vector<layout::LegendEntry> entries;
        entries.reserve(layers.size());
        for (const Series* s : layers) {
            entries.push_back(layout::LegendEntry{s->name, s->color});
        }
        layout::draw_legend(ctx.canvas, entries, Rect{inner.x, 0.0, inner.w, 1.0},
                            layout::HAlign::Left);
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

}  // namespace termchart::chart
