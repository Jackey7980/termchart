// roc.cpp —— ROC/PR 实现：降序阈值扫描 + 梯形 AUC
#include "termchart/chart/statistical/roc.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <vector>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/legend.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"

namespace termchart::chart {
namespace {

// 梯形积分（曲线按首坐标升序给点）。
double trapezoid(const std::vector<Point>& pts) {
    double area = 0.0;
    for (std::size_t i = 1; i < pts.size(); ++i) {
        const double dx = pts[i].x - pts[i - 1].x;
        area += dx * (pts[i].y + pts[i - 1].y) * 0.5;
    }
    return area;
}

}  // namespace

void RocChart::set_scores(std::vector<double> scores, std::vector<int> labels,
                          std::string name) {
    const std::size_t n = std::min(scores.size(), labels.size());
    scores_.clear();
    labels_.clear();
    scores_.reserve(n);
    labels_.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (std::isfinite(scores[i])) {
            scores_.push_back(scores[i]);
            labels_.push_back(labels[i]);
        }
    }
    name_ = std::move(name);

    roc_.clear();
    pr_.clear();
    auc_roc_ = 0.0;
    auc_pr_ = 0.0;

    std::vector<std::size_t> order(scores_.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return scores_[a] > scores_[b];  // 分数降序；同分保持稳定序
    });

    long tp = 0;
    long fp = 0;
    long positives = 0;
    long negatives = 0;
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (labels_[order[i]] > 0) {
            ++positives;
        } else {
            ++negatives;
        }
    }
    if (positives == 0 || negatives == 0) {
        return;  // 缺任一类：两指标均无定义
    }

    // 同分合并为同一阈值：一组一并计入后再落点。
    std::size_t i = 0;
    while (i < order.size()) {
        std::size_t j = i;
        while (j < order.size() && scores_[order[j]] == scores_[order[i]]) {
            if (labels_[order[j]] > 0) {
                ++tp;
            } else {
                ++fp;
            }
            ++j;
        }
        const double fpr = static_cast<double>(fp) / static_cast<double>(negatives);
        const double tpr = static_cast<double>(tp) / static_cast<double>(positives);
        roc_.push_back(Point{fpr, tpr});
        const double recall = tpr;
        const double precision = (tp + fp) > 0
                                     ? static_cast<double>(tp) / static_cast<double>(tp + fp)
                                     : 1.0;
        pr_.push_back(Point{recall, precision});
        i = j;
    }
    auc_roc_ = trapezoid(roc_);
    auc_pr_ = trapezoid(pr_);
}

void RocChart::draw(DrawContext ctx) {
    const std::vector<Point>& curve =
        (options_.mode == RocMode::Pr) ? pr_ : roc_;
    if (curve.empty()) {
        return;
    }
    const layout::Margin m{margin_top_, margin_right_, margin_bottom_, margin_left_};
    area_ = make_area(ctx.canvas, m, 0.0, 1.0, 0.0, 1.0, false, false);

    const std::vector<double> xticks = scale::ticks_for(area_.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area_.y_scale());
    if (ctx.draw_grid) {
        const Style grid_style = ctx.grid_style.fg().is_none()
                                     ? Style{}.set_fg(Color::ansi16(8))
                                     : ctx.grid_style;
        layout::draw_grid(ctx.canvas, area_, xticks, yticks, grid_style);
    }
    if (ctx.draw_axes) {
        const bool pr = options_.mode == RocMode::Pr;
        layout::draw_axis_x(ctx.canvas, area_, xticks, pr ? "recall" : "fpr",
                            ctx.axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks,
                            pr ? "precision" : "tpr", ctx.axis_style);
    }

    const Style st = Style{}.set_fg(Color::ansi16(4));
    const Rect inner = area_.inner_rect();
    if (options_.mode == RocMode::Roc && options_.show_diagonal) {
        // 随机分类器基线：(0,0) → (1,1)。
        ctx.canvas.draw_line(Point{inner.x, inner.bottom()},
                             Point{inner.right(), inner.y}, U'\u00B7',
                             Style{}.set_fg(Color::ansi16(8)));
    }
    for (std::size_t i = 0; i < curve.size(); ++i) {
        const Point cell = area_.cell_of(curve[i].x, curve[i].y);
        if (i > 0) {
            const Point prev = area_.cell_of(curve[i - 1].x, curve[i - 1].y);
            ctx.canvas.draw_line(prev, cell, U'\u2022', st);
        }
        ctx.canvas.put(cell, kMarkerChar, st);
    }

    if (show_legend_ && ctx.draw_legend && !name_.empty()) {
        std::vector<layout::LegendEntry> entries;
        if (options_.mode == RocMode::Roc) {
            entries.push_back(layout::LegendEntry{
                name_ + " (AUC=" + layout::format_tick(auc_roc_) + ")", Color::ansi16(4)});
        } else {
            entries.push_back(layout::LegendEntry{
                name_ + " (AUC=" + layout::format_tick(auc_pr_) + ")", Color::ansi16(4)});
        }
        const Rect legend_rect{inner.x, 0.0, inner.w, 1.0};
        layout::draw_legend(ctx.canvas, entries, legend_rect, layout::HAlign::Left);
    }
    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

}  // namespace termchart::chart
