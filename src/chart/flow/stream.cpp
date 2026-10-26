// stream.cpp —— 流向图实现：逐列占比堆叠（ThemeRiver / 堆叠两种基线）
#include "termchart/chart/flow/stream.hpp"

#include <algorithm>

#include "termchart/chart/hierarchical/node.hpp"
#include "termchart/chart/scene_sink.hpp"
#include "termchart/layout/legend.hpp"
#include "termchart/layout/title.hpp"

namespace termchart::chart {

void StreamChart::set_series(std::vector<Series> series) {
    series_ = std::move(series);
}

void StreamChart::draw(DrawContext ctx) {
    if (series_.empty()) {
        return;
    }
    canvas::Canvas& canvas = ctx.canvas;
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect inner{m.left, m.top,
                     static_cast<double>(canvas.width()) - m.left - m.right,
                     static_cast<double>(canvas.height()) - m.top - m.bottom};
    if (inner.w < 2.0 || inner.h < 2.0) {
        return;
    }

    const std::size_t n = series_.front().points.size();
    if (n == 0) {
        return;
    }
    std::vector<double> totals(n, 0.0);
    for (const Series& s : series_) {
        for (std::size_t i = 0; i < s.points.size() && i < n; ++i) {
            const double v = s.points[i].y > 0.0 ? s.points[i].y : 0.0;
            totals[i] += v;
        }
    }
    double col_max = 0.0;
    for (double t : totals) {
        col_max = std::max(col_max, t);
    }
    if (col_max <= 0.0) {
        return;
    }

    for (std::size_t i = 0; i < n; ++i) {
        const double col_w = inner.w / static_cast<double>(n);
        const double x0 = inner.x + static_cast<double>(i) * col_w;
        const double h = inner.h * (totals[i] / col_max);
        double acc = options.centered ? (inner.y + (inner.h - h) / 2.0)
                                      : inner.bottom() - h;
        for (std::size_t k = 0; k < series_.size(); ++k) {
            const Series& s = series_[k];
            if (i >= s.points.size()) {
                continue;
            }
            const double v = s.points[i].y > 0.0 ? s.points[i].y : 0.0;
            const double seg_h = h * (v / totals[i]);
            const Color color =
                s.color.is_none() ? hier::hier_palette(k) : s.color;
            canvas.fill_rect(Rect{x0, acc, col_w, seg_h}, kBarChar,
                             Style{}.set_fg(color));
            acc += seg_h;
        }
    }

    if (ctx.draw_legend) {
        std::vector<layout::LegendEntry> entries;
        entries.reserve(series_.size());
        for (std::size_t k = 0; k < series_.size(); ++k) {
            const Color color =
                series_[k].color.is_none() ? hier::hier_palette(k) : series_[k].color;
            entries.push_back(layout::LegendEntry{series_[k].name, color});
        }
        layout::draw_legend(canvas, entries, Rect{inner.x, 0.0, inner.w, 1.0},
                            layout::HAlign::Left);
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(canvas, title_, inner);
    }
}

bool StreamChart::to_scene(scene::Scene& out, std::uint32_t width,
                           std::uint32_t height) const {
    // 与 draw() 同源：空系列/零可见区域直接返回成功（不画任何图元）。
    if (series_.empty()) {
        return true;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    const Rect inner{m.left, m.top,
                     static_cast<double>(width) - m.left - m.right,
                     static_cast<double>(height) - m.top - m.bottom};
    if (inner.w < 2.0 || inner.h < 2.0) {
        return true;
    }

    const std::size_t n = series_.front().points.size();
    if (n == 0) {
        return true;
    }
    std::vector<double> totals(n, 0.0);
    for (const Series& s : series_) {
        for (std::size_t i = 0; i < s.points.size() && i < n; ++i) {
            const double v = s.points[i].y > 0.0 ? s.points[i].y : 0.0;
            totals[i] += v;
        }
    }
    double col_max = 0.0;
    for (double t : totals) {
        col_max = std::max(col_max, t);
    }
    if (col_max <= 0.0) {
        return true;
    }

    SceneSink sink(out);
    scene::SceneBuilder& b = sink.builder();
    b.begin_group(0);
    for (std::size_t i = 0; i < n; ++i) {
        const double col_w = inner.w / static_cast<double>(n);
        const double x0 = inner.x + static_cast<double>(i) * col_w;
        const double h = inner.h * (totals[i] / col_max);
        double acc = options.centered ? (inner.y + (inner.h - h) / 2.0)
                                      : inner.bottom() - h;
        for (std::size_t k = 0; k < series_.size(); ++k) {
            const Series& s = series_[k];
            if (i >= s.points.size()) {
                continue;
            }
            const double v = s.points[i].y > 0.0 ? s.points[i].y : 0.0;
            const double seg_h = h * (v / totals[i]);
            const Color color =
                s.color.is_none() ? hier::hier_palette(k) : s.color;
            b.fill(color);
            b.rect(Rect{x0, acc, col_w, seg_h});
            acc += seg_h;
        }
    }

    std::vector<layout::LegendEntry> entries;
    entries.reserve(series_.size());
    for (std::size_t k = 0; k < series_.size(); ++k) {
        const Color color =
            series_[k].color.is_none() ? hier::hier_palette(k) : series_[k].color;
        entries.push_back(layout::LegendEntry{series_[k].name, color});
    }
    layout::draw_legend(sink, entries, Rect{inner.x, 0.0, inner.w, 1.0},
                        layout::HAlign::Left);

    if (!title_.empty()) {
        layout::draw_title(sink, title_, inner);
    }
    b.end_group(0);
    return true;
}

}  // namespace termchart::chart
