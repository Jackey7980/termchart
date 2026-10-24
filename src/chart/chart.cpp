// chart.cpp —— Chart 基类虚表 + 折线/柱状图共享几何辅助
#include "termchart/chart/chart.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "termchart/layout/legend.hpp"
#include "termchart/scale/auto_domain.hpp"

namespace termchart::chart {

Chart::~Chart() = default;

void data_bounds(const std::vector<Series>& series, double& xmin, double& xmax,
                 double& ymin, double& ymax) {
    xmin = ymin = std::numeric_limits<double>::infinity();
    xmax = ymax = -std::numeric_limits<double>::infinity();
    for (const auto& s : series) {
        for (const Point& p : s.points) {
            xmin = std::min(xmin, p.x);
            xmax = std::max(xmax, p.x);
            ymin = std::min(ymin, p.y);
            ymax = std::max(ymax, p.y);
        }
    }
}

layout::PlotArea make_area(canvas::Canvas& canvas, const layout::Margin& margin,
                           double xmin, double xmax, double ymin, double ymax,
                           bool auto_domain, bool include_zero,
                           std::optional<scale::Domain> y_override) {
    const double w = static_cast<double>(canvas.width());
    const double h = static_cast<double>(canvas.height());
    if (!auto_domain) {
        return layout::PlotArea(w, h, margin, xmin, xmax, ymin, ymax);
    }
    const scale::Domain xd = scale::nice_domain(xmin, xmax, false);
    if (y_override.has_value()) {
        // Y 用权威域、X 保持原有 nice 规整——调用方只买"轴域不许取极值"这一项，
        // 不该顺带改掉 X 的外观。退化/非有限域当没给（否则比例尺 span=0 → 除零）。
        const double lo = y_override->min;
        const double hi = y_override->max;
        if (std::isfinite(lo) && std::isfinite(hi) && hi > lo) {
            return layout::PlotArea(w, h, margin, xd.min, xd.max, lo, hi);
        }
    }
    const scale::Domain yd = scale::nice_domain(ymin, ymax, include_zero);
    return layout::PlotArea(w, h, margin, xd.min, xd.max, yd.min, yd.max);
}

void draw_legend_for(canvas::Canvas& canvas, const std::vector<Series>& series,
                     const layout::PlotArea& area, bool show) {
    if (!show || series.empty()) {
        return;
    }
    std::vector<layout::LegendEntry> entries;
    entries.reserve(series.size());
    for (const auto& s : series) {
        entries.push_back(layout::LegendEntry{s.name, s.color});
    }
    const Rect inner = area.inner_rect();
    const Rect legend_rect{inner.x, 0.0, inner.w, 1.0};
    layout::draw_legend(canvas, entries, legend_rect, layout::HAlign::Left);
}

}  // namespace termchart::chart
