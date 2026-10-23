// boxplot.cpp —— 箱线图实现（五数概括 → 箱/须/中位/离群点）
#include "termchart/chart/cartesian/boxplot.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "termchart/layout/axis.hpp"
#include "termchart/layout/grid.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/ticks.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::chart {
namespace {

inline constexpr char32_t kBoxChar = U'\u2588';      // █ 箱体
inline constexpr char32_t kMedianChar = U'\u2500';   // ─ 中位线与须端帽
inline constexpr char32_t kWhiskerChar = U'\u2502';  // │ 须
inline constexpr char32_t kOutlierChar = U'\u25CB';  // ○ 离群点

bool valid_group(const BoxPlotChart::BoxStats& g) noexcept {
    return std::isfinite(g.lo) && std::isfinite(g.q1) && std::isfinite(g.med) &&
           std::isfinite(g.q3) && std::isfinite(g.hi) && g.lo <= g.q1 &&
           g.q1 <= g.med && g.med <= g.q3 && g.q3 <= g.hi;
}

}  // namespace

void BoxPlotChart::draw(DrawContext ctx) {
    // 无效组整体剔除，但保留**原始下标**——labels_ 按 set_data 的次序对齐，
    // 剔除后若用紧凑下标贴标签会串组。
    std::vector<std::pair<std::size_t, BoxStats>> groups;
    groups.reserve(groups_.size());
    for (std::size_t i = 0; i < groups_.size(); ++i) {
        if (valid_group(groups_[i])) {
            groups.emplace_back(i, groups_[i]);
        }
    }
    if (groups.empty()) {
        return;
    }

    // y 域把离群点也算进来（它们必须可见），非有限的离群点直接丢。
    double ymin = groups.front().second.lo;
    double ymax = groups.front().second.hi;
    for (const auto& [index, g] : groups) {
        ymin = std::min(ymin, g.lo);
        ymax = std::max(ymax, g.hi);
        for (double o : g.outliers) {
            if (std::isfinite(o)) {
                ymin = std::min(ymin, o);
                ymax = std::max(ymax, o);
            }
        }
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
        // 有分组标签时 x 轴标题留空（标签即轴标题，避免与 "group" 叠字）。
        const char* x_title = labels_.empty() ? "group" : "";
        layout::draw_axis_x(ctx.canvas, area_, xticks, x_title, axis_style);
        layout::draw_axis_y(ctx.canvas, area_, yticks, "value", axis_style);
    }

    const Style st = Style{}.set_fg(opts_.color);
    const Rect inner = area_.inner_rect();
    const double bw = std::max(1.0, inner.w / n * 0.6);

    for (std::size_t i = 0; i < groups.size(); ++i) {
        const auto& [original, g] = groups[i];
        const double xc = area_.x_scale().forward(static_cast<double>(i));
        const double y_lo = area_.y_scale().forward(g.lo);
        const double y_q1 = area_.y_scale().forward(g.q1);
        const double y_med = area_.y_scale().forward(g.med);
        const double y_q3 = area_.y_scale().forward(g.q3);
        const double y_hi = area_.y_scale().forward(g.hi);
        const double left = xc - bw / 2.0;

        // 须：箱体上下边到须端（先画，箱体会盖住重合段）。
        if (g.hi > g.q3) {
            ctx.canvas.draw_line(Point{xc, y_hi}, Point{xc, y_q3}, kWhiskerChar, st);
        }
        if (g.lo < g.q1) {
            ctx.canvas.draw_line(Point{xc, y_q1}, Point{xc, y_lo}, kWhiskerChar, st);
        }
        // 须端帽。
        ctx.canvas.fill_rect(Rect{left, y_hi, bw, 1.0}, kMedianChar, st);
        ctx.canvas.fill_rect(Rect{left, y_lo, bw, 1.0}, kMedianChar, st);
        // 箱体（q1–q3，至少 1 行）+ 中位线覆盖中行。
        ctx.canvas.fill_rect(Rect{left, y_q3, bw, std::max(1.0, y_q1 - y_q3)}, kBoxChar, st);
        ctx.canvas.fill_rect(Rect{left, y_med, bw, 1.0}, kMedianChar, st);

        if (opts_.show_outliers) {
            for (double o : g.outliers) {
                if (std::isfinite(o)) {
                    ctx.canvas.put(area_.cell_of(static_cast<double>(i), o), kOutlierChar, st);
                }
            }
        }

        // 分组标签：按**原始下标**取，居中贴在箱体下方一行的轴标签带。
        if (original < labels_.size() && !labels_[original].empty()) {
            const std::string& label = labels_[original];
            const double label_w =
                static_cast<double>(util::display_width(label));
            ctx.canvas.draw_text(Point{xc - label_w / 2.0, inner.y + inner.h + 1.0}, label,
                                 axis_style);
        }
    }

    if (ctx.draw_title && !title_.empty()) {
        layout::draw_title(ctx.canvas, title_, inner);
    }
}

}  // namespace termchart::chart
