// chart.cpp —— Chart 基类虚表 + 折线/柱状图共享几何辅助
#include "termchart/chart/chart.hpp"

#include <algorithm>
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
                           bool auto_domain, bool include_zero) {
    if (auto_domain) {
        const scale::Domain yd = scale::nice_domain(ymin, ymax, include_zero);
        const scale::Domain xd = scale::nice_domain(xmin, xmax, false);
        return layout::PlotArea(static_cast<double>(canvas.width()),
                                static_cast<double>(canvas.height()), margin, xd.min,
                                xd.max, yd.min, yd.max);
    }
    return layout::PlotArea(static_cast<double>(canvas.width()),
                            static_cast<double>(canvas.height()), margin, xmin, xmax,
                            ymin, ymax);
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
