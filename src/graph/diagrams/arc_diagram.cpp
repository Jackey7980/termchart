// arc_diagram.cpp —— 弧线图实现
#include "termchart/graph/diagrams/arc_diagram.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/util/wcwidth.hpp"
#include "internal.hpp"

namespace termchart::graph {
namespace {

constexpr double kMaxSamples = 8192.0;

// 采样二次贝塞尔并去重成格序列（与 chord.cpp 同一做法：步数按弦长估计）。
std::vector<Point> quadratic_cells(Point p0, Point p1, Point p2) {
    const double chord = std::hypot(p2.x - p0.x, p2.y - p0.y);
    std::size_t steps = 2;
    if (chord > 0.5) {
        const double raw = std::ceil(chord * 4.0);
        steps = raw >= kMaxSamples ? static_cast<std::size_t>(kMaxSamples)
                                   : static_cast<std::size_t>(raw);
    }
    std::vector<Point> cells;
    cells.reserve(steps + 1);
    for (std::size_t k = 0; k <= steps; ++k) {
        const double t = static_cast<double>(k) / static_cast<double>(steps);
        const double u = 1.0 - t;
        const Point s{u * u * p0.x + 2.0 * u * t * p1.x + t * t * p2.x,
                      u * u * p0.y + 2.0 * u * t * p1.y + t * t * p2.y};
        const Point c{std::round(s.x), std::round(s.y)};
        if (cells.empty() || cells.back() != c) {
            cells.push_back(c);
        }
    }
    return cells;
}

}  // namespace

Rect render_arc_diagram(const std::vector<ArcNode>& nodes,
                        const std::vector<ArcLink>& links, canvas::Canvas& canvas,
                        const ArcDiagramOptions& opts) {
    const std::size_t n = nodes.size();
    if (n == 0 || canvas.height() == 0) {
        return Rect{};
    }

    // 1) 列分配：按登记顺序一字排开，标签居中于各自的列。
    const double gap = std::max(1.0, opts.h_gap);
    std::vector<double> col(n, 0.0);
    std::vector<double> width(n, 0.0);
    double cursor = 1.0;
    for (std::size_t i = 0; i < n; ++i) {
        width[i] = static_cast<double>(util::display_width(nodes[i].label));
        col[i] = cursor + width[i] / 2.0;
        cursor += width[i] + gap;
    }

    // 2) 节点行贴住画布底行。
    const double base = static_cast<double>(canvas.height()) - 1.0;
    Rect bounds{};
    for (std::size_t i = 0; i < n; ++i) {
        if (width[i] <= 0.0) {
            continue;
        }
        const double x = std::round(col[i] - width[i] / 2.0);
        canvas.draw_text(Point{x, base}, nodes[i].label, opts.label_style);
        bounds = bounds.union_with(Rect{x, base, width[i], 1.0});
    }

    // 3) 弧：跨度越大弧顶越高（二次贝塞尔，控制点抬升两倍目标高度使顶点恰为 h）。
    const double max_h =
        std::max(1.0, static_cast<double>(std::max(1, opts.max_height)));
    for (const ArcLink& l : links) {
        if (l.from >= n || l.to >= n) {
            continue;
        }
        double h = 1.0;
        Point p0{};
        Point p2{};
        if (l.from == l.to) {
            // 自连接：跨标签宽的小拱。
            const double half = std::max(1.0, width[l.from] / 2.0);
            p0 = Point{col[l.from] - half, base - 1.0};
            p2 = Point{col[l.from] + half, base - 1.0};
            h = 1.0;
        } else {
            const double span = std::fabs(col[l.to] - col[l.from]);
            h = std::min(max_h, std::max(1.0, std::ceil(span / 2.0)));
            p0 = Point{col[l.from], base - 1.0};
            p2 = Point{col[l.to], base - 1.0};
        }
        const Point p1{(p0.x + p2.x) / 2.0, p0.y - 2.0 * h};
        const std::vector<Point> cells = quadratic_cells(p0, p1, p2);
        for (std::size_t k = 1; k < cells.size(); ++k) {
            canvas.draw_line(cells[k - 1], cells[k],
                             detail::segment_char(cells[k - 1], cells[k]),
                             opts.arc_style);
        }
        const double lo = std::min(p0.x, p2.x);
        const double hi = std::max(p0.x, p2.x);
        bounds = bounds.union_with(Rect{lo, base - 1.0 - h, hi - lo + 1.0, h + 1.0});
    }
    return bounds;
}

}  // namespace termchart::graph
