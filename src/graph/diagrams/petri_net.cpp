// petri_net.cpp —— Petri 网渲染：深度分列布局 + 正交弧 + token
#include "termchart/graph/diagrams/petri_net.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/wiregrid.hpp"
#include "termchart/util/wcwidth.hpp"
#include "internal.hpp"

namespace termchart::graph {

namespace {

constexpr char32_t kTokenDot = U'\u25CF';  // ● token
constexpr double kPlaceMinW = 7.0;         // 椭圆太扁就失去形状了
constexpr double kNodeH = 5.0;             // 库所/变迁统一 5 行高
constexpr double kTransitionW = 3.0;       // 变迁 = 窄高矩形（经典画法）

}  // namespace

Rect render_petri_net(const PetriNet& net, canvas::Canvas& canvas,
                      const PetriNetOptions& opts) {
    const std::size_t place_count = net.places.size();
    const std::size_t node_count = place_count + net.transitions.size();
    if (node_count == 0) {
        return Rect{};
    }

    // 库所/变迁统一成"节点列表"：前 P 项是库所，后 T 项是变迁。
    struct NetNode {
        bool is_place = true;
        std::size_t index = 0;  // 在 places/transitions 里的下标
        std::string label;
        Rect size{};
        std::size_t depth = 0;
        Rect box{};
    };
    std::vector<NetNode> nodes(node_count);
    for (std::size_t i = 0; i < place_count; ++i) {
        NetNode& n = nodes[i];
        n.is_place = true;
        n.index = i;
        n.label = net.places[i].name;
        const double label_w = static_cast<double>(util::display_width(n.label));
        n.size = Rect{0.0, 0.0, std::max(kPlaceMinW, label_w + 2.0), kNodeH};
    }
    for (std::size_t j = 0; j < net.transitions.size(); ++j) {
        NetNode& n = nodes[place_count + j];
        n.is_place = false;
        n.index = j;
        n.label = net.transitions[j].name;
        n.size = Rect{0.0, 0.0, kTransitionW, kNodeH};
    }

    // 弧 → 节点对（端点越界或自环的弧跳过）。
    std::vector<std::pair<std::size_t, std::size_t>> arc_nodes;
    arc_nodes.reserve(net.arcs.size());
    for (const PetriNet::Arc& arc : net.arcs) {
        const std::size_t from = arc.from.is_place ? arc.from.index
                                                   : place_count + arc.from.index;
        const std::size_t to = arc.to.is_place ? arc.to.index
                                               : place_count + arc.to.index;
        if (from >= node_count || to >= node_count || from == to) {
            continue;
        }
        arc_nodes.emplace_back(from, to);
    }

    // 弧可达深度：d[v] = min(入边 u 的 d[u]) + 1；无入弧的节点为 0。有环时
    // 迭代有界；弧只在库所/变迁之间，有限深度的奇偶性稳定（库所偶、变迁奇）。
    // 从源不可达的节点（纯环成员）按种类落到奇偶相容的兜底列。
    {
        const std::size_t kUnknown = node_count + 1;
        std::vector<std::size_t> indegree(node_count, 0);
        for (const auto& [u, v] : arc_nodes) {
            (void)u;
            ++indegree[v];
        }
        for (std::size_t i = 0; i < node_count; ++i) {
            nodes[i].depth = indegree[i] == 0 ? 0 : kUnknown;
        }
        bool changed = true;
        for (std::size_t iter = 0; changed && iter <= node_count; ++iter) {
            changed = false;
            for (const auto& [u, v] : arc_nodes) {
                if (nodes[u].depth == kUnknown) {
                    continue;
                }
                const std::size_t next = nodes[u].depth + 1;
                if (nodes[v].depth > next) {
                    nodes[v].depth = next;
                    changed = true;
                }
            }
        }
        for (NetNode& n : nodes) {
            if (n.depth == kUnknown) {
                n.depth = n.is_place ? 0 : 1;
            }
        }
    }

    // 分列：x 逐列累计（各列取本列最大宽度），列内按登记顺序纵向堆叠。
    std::size_t col_count = 0;
    for (const NetNode& n : nodes) {
        col_count = std::max(col_count, n.depth + 1);
    }
    std::vector<double> col_w(col_count, 1.0);
    for (const NetNode& n : nodes) {
        col_w[n.depth] = std::max(col_w[n.depth], n.size.w);
    }
    std::vector<double> col_x(col_count, 0.0);
    for (std::size_t c = 1; c < col_count; ++c) {
        col_x[c] = col_x[c - 1] + col_w[c - 1] + opts.layout.h_gap;
    }
    std::vector<double> col_y(col_count, opts.layout.margin);
    for (NetNode& n : nodes) {
        n.box = Rect{col_x[n.depth], col_y[n.depth], n.size.w, n.size.h};
        col_y[n.depth] += n.size.h + opts.layout.v_gap;
    }

    // 建临时 Graph 只为复用 route_edges（含节点障碍登记与锚点分配）。
    Graph graph;
    std::vector<NodeId> ids(node_count);
    for (std::size_t i = 0; i < node_count; ++i) {
        Node& node = graph.add_node(nodes[i].label);
        node.set_box(nodes[i].box);
        ids[i] = node.id();
    }
    for (const auto& [u, v] : arc_nodes) {
        (void)graph.add_edge(ids[u], ids[v]);
    }
    {
        WireGrid grid(canvas.width(), canvas.height());
        (void)route_edges(graph, RouteStyle::Orthogonal, opts.route, &grid);
    }

    // 1) 弧（先画线，节点与标签后画才有覆盖优先级）。
    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        for (std::size_t s = 1; s < route.size(); ++s) {
            canvas.draw_line(route[s - 1], route[s],
                             detail::segment_char(route[s - 1], route[s]),
                             opts.arc_style);
        }
    }

    // 2) 形状与 token。
    for (std::size_t i = 0; i < node_count; ++i) {
        const NetNode& n = nodes[i];
        if (n.is_place) {
            draw_shape(canvas, ShapeKind::Ellipse, n.box, opts.place_style);
            const PetriNet::Place& place = net.places[n.index];
            if (place.tokens > 0) {
                const double cy =
                    n.box.y + std::floor(std::max(0.0, n.box.h - 1.0) / 2.0);
                if (place.tokens <= opts.max_token_dots) {
                    const double w = static_cast<double>(place.tokens);
                    double x =
                        n.box.x + std::floor(std::max(0.0, n.box.w - w) / 2.0);
                    for (std::uint32_t k = 0; k < place.tokens; ++k) {
                        canvas.put(Point{x, cy}, kTokenDot, opts.place_style);
                        x += 1.0;
                    }
                } else {
                    const std::string text = std::to_string(place.tokens);
                    const double w =
                        static_cast<double>(util::display_width(text));
                    const double x =
                        n.box.x + std::floor(std::max(0.0, n.box.w - w) / 2.0);
                    canvas.draw_text(Point{x, cy}, text, opts.place_style);
                }
            }
        } else {
            draw_shape(canvas, ShapeKind::Box, n.box, opts.transition_style);
        }
    }

    // 3) 箭头（最后画，否则被节点边框覆盖）。
    for (const Edge& edge : graph.edges()) {
        const std::vector<Point>& route = edge.route();
        if (route.size() < 2) {
            continue;
        }
        detail::put_arrow(canvas, route[route.size() - 2], route.back(),
                          opts.arc_style);
    }

    // 4) 名称画在形状下方（形状高 5、列内间隔 ≥ 2，下方留有一行空档）。
    Rect bounds{};
    for (const NetNode& n : nodes) {
        bounds = bounds.union_with(n.box);
        if (n.label.empty()) {
            continue;
        }
        canvas.draw_text(Point{n.box.x, n.box.bottom()}, n.label, opts.place_style);
        bounds = bounds.union_with(Rect{n.box.x, n.box.bottom(), n.box.w, 1.0});
    }
    return bounds;
}

}  // namespace termchart::graph
