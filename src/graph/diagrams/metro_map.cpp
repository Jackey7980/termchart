// metro_map.cpp —— 地铁线路图渲染：建站合并 + 45° 布局走线 + 线色
#include "termchart/graph/diagrams/metro_map.hpp"

#include <cstddef>
#include <map>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/util/wcwidth.hpp"
#include "internal.hpp"

namespace termchart::graph {

Rect render_metro_map(const std::vector<MetroLine>& lines, canvas::Canvas& canvas,
                      const MetroMapOptions& opts) {
    // 建图：同名站点合并为换乘站；线路的相邻站点成边。
    Graph graph;
    std::map<std::string, NodeId> station_ids;
    std::vector<std::size_t> line_of_edge;
    line_of_edge.reserve(lines.size() * 4);
    for (std::size_t li = 0; li < lines.size(); ++li) {
        const MetroLine& line = lines[li];
        std::vector<NodeId> ids;
        ids.reserve(line.stations.size());
        for (const std::string& name : line.stations) {
            const auto it = station_ids.find(name);
            if (it != station_ids.end()) {
                ids.push_back(it->second);
                continue;
            }
            const NodeId id = graph.add_node(name).id();
            (void)station_ids.emplace(name, id);
            ids.push_back(id);
        }
        for (std::size_t k = 1; k < ids.size(); ++k) {
            if (ids[k - 1] == ids[k]) {
                continue;  // 相邻同名站（数据毛病）：跳过
            }
            if (graph.add_edge(ids[k - 1], ids[k]) == nullptr) {
                continue;
            }
            line_of_edge.push_back(li);
        }
    }
    if (graph.nodes().empty()) {
        return Rect{};
    }

    const Rect bounds = layout_metro(graph, opts.metro);
    (void)route_metro(graph, MetroRouteOptions{}, nullptr);

    // 1) 走线（先线后站；不画箭头——地铁区间无方向语义）。
    const std::vector<Edge>& edges = graph.edges();
    for (std::size_t i = 0; i < edges.size(); ++i) {
        Style style = opts.fallback_line_style;
        const std::size_t li = line_of_edge[i];
        if (li < lines.size() && !lines[li].color.is_none()) {
            style.set_fg(lines[li].color);
        }
        const std::vector<Point>& route = edges[i].route();
        for (std::size_t s = 1; s < route.size(); ++s) {
            canvas.draw_line(route[s - 1], route[s],
                             detail::segment_char(route[s - 1], route[s]), style);
        }
    }

    // 2) 站点（边框 + 居中标名，与共享骨架的节点画法一致）。
    for (const Node& node : graph.nodes()) {
        if (opts.station_borders) {
            draw_shape(canvas, ShapeKind::Box, node.box(), opts.station_style);
        }
        if (node.label().empty()) {
            continue;
        }
        const int label_width = util::display_width(node.label());
        const double x = node.box().x + std::max(
            0.0, (node.box().w - static_cast<double>(label_width)) / 2.0);
        const double y =
            node.box().y + std::floor(std::max(0.0, node.box().h - 1.0) / 2.0);
        canvas.draw_text(Point{x, y}, node.label(), opts.station_style);
    }
    return bounds;
}

}  // namespace termchart::graph
