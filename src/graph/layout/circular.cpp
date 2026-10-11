// circular.cpp —— 圆周布局实现
#include "termchart/graph/layout/circular.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace termchart::graph {

namespace {

constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kPi = 3.14159265358979323846;

Rect union_of_boxes(const Graph& graph) noexcept {
    if (graph.nodes().empty()) {
        return Rect{};
    }
    double min_x = graph.nodes().front().box().x;
    double min_y = graph.nodes().front().box().y;
    double max_x = graph.nodes().front().box().right();
    double max_y = graph.nodes().front().box().bottom();
    for (const Node& n : graph.nodes()) {
        min_x = std::min(min_x, n.box().x);
        min_y = std::min(min_y, n.box().y);
        max_x = std::max(max_x, n.box().right());
        max_y = std::max(max_y, n.box().bottom());
    }
    return Rect{min_x, min_y, max_x - min_x, max_y - min_y};
}

}  // namespace

Rect layout_circular(Graph& graph, const CircularOptions& opts) {
    const std::size_t count = graph.nodes().size();
    if (count == 0) {
        return Rect{};
    }

    std::vector<Rect> size(count);
    for (std::size_t i = 0; i < count; ++i) {
        size[i] = node_box_for(graph.nodes()[i], opts.layout);
    }

    const double aspect = std::max(0.01, opts.aspect);

    // 半径：n ≥ 2 时由节点数与尺寸推出；n = 1 时取 0（单节点落在圆心）。
    double radius = 0.0;
    if (count >= 2) {
        const auto n = static_cast<double>(count);
        // 相邻一对外接圆半径之和的最大值 + 期望间距。用外接圆是保守判据：ρ 圆不相交
        // ⇒ 矩形必不相交，且不受矩形朝向影响（绝不把半径算小）。
        double max_pair = 0.0;
        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t j = (i + 1) % count;
            const double rho_i = std::hypot(size[i].w, size[i].h) / 2.0;
            const double rho_j = std::hypot(size[j].w, size[j].h) / 2.0;
            max_pair = std::max(max_pair, rho_i + rho_j);
        }
        const double needed = max_pair + std::max(0.0, opts.gap);
        // 最紧方向的相邻中心格距离为 2·(R/aspect)·sin(π/n)；令其 ≥ needed 解出 R。
        const double sin_half = std::sin(kPi / n);
        radius = aspect * needed / (2.0 * sin_half);
        radius = std::max(radius, std::max(0.0, opts.min_radius));
    }

    // 极坐标 → 格坐标（y 压缩 1/aspect，口径同 canvas/transform.hpp）。
    std::vector<double> px(count, 0.0);
    std::vector<double> py(count, 0.0);
    for (std::size_t i = 0; i < count; ++i) {
        const double theta =
            opts.start_angle + kTwoPi * static_cast<double>(i) / static_cast<double>(count);
        px[i] = radius * std::cos(theta);
        py[i] = radius * std::sin(theta) / aspect;
    }

    // 落格：与 force/radial 同法，保证所有框落在返回 Rect 内且最小边贴 margin。
    double min_x = px[0] - size[0].w / 2.0;
    double min_y = py[0] - size[0].h / 2.0;
    for (std::size_t i = 0; i < count; ++i) {
        min_x = std::min(min_x, px[i] - size[i].w / 2.0);
        min_y = std::min(min_y, py[i] - size[i].h / 2.0);
    }
    for (std::size_t i = 0; i < count; ++i) {
        const double x =
            std::round(px[i] - size[i].w / 2.0 - min_x) + opts.layout.margin;
        const double y =
            std::round(py[i] - size[i].h / 2.0 - min_y) + opts.layout.margin;
        graph.nodes()[i].set_box(Rect{x, y, size[i].w, size[i].h});
    }
    return union_of_boxes(graph);
}

}  // namespace termchart::graph
