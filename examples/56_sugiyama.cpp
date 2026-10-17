// 56_sugiyama.cpp —— Sugiyama 分层图：分层 → 层内降交叉 → 坐标分配 → 正交路由 → 字符画
//
// 布局四步**全部**走库的真实现，示例内不再手写任何布局算法：
//   ① 分层      graph::layout_sugiyama 内部：最长路径定层（含破环）
//   ② 降交叉    graph::layout_sugiyama 内部：median 启发式双向扫描（默认 4 轮）
//   ③ 坐标分配  graph::layout_sugiyama 内部：层 = 列，层内按降交叉后的顺序纵向排列
//   ④ 边路由    graph::route_edges(RouteStyle::Orthogonal) + WireGrid 避障
// 绘制部分用库原语：graph/shape 画框、canvas 画折线、arrow_head_char 画箭头。
//
// 降交叉的收益用 graph::count_crossings（节点中心连线段）量化：同一张图先走
// graph::layout_layered（同样分层，但层内按登记顺序、不做降交叉）拿基线，再走
// layout_sugiyama；两次都是库实现，数的都是摆好坐标后的真实几何。
//
// 演示图故意按"必然交叉"的顺序登记：L0 的 A/B/C 与 L1 的 D/E/F 逆序相连。
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/graph/arrow.hpp"
#include "termchart/graph/crossing.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"
#include "termchart/graph/layout/sugiyama.hpp"
#include "termchart/graph/port.hpp"
#include "termchart/graph/route/route.hpp"
#include "termchart/graph/shape/shape.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/term/terminal.hpp"
#include "termchart/util/wcwidth.hpp"

namespace {

using namespace termchart;

// ── 度量：降交叉到底减少了多少交叉 ──────────────────────────────────────
std::size_t count_center_crossings(const graph::Graph& graph) {
    std::vector<std::pair<Point, Point>> segments;
    segments.reserve(graph.edge_count());
    for (const graph::Edge& edge : graph.edges()) {
        const graph::Node* from = graph.find(edge.from());
        const graph::Node* to = graph.find(edge.to());
        if (from == nullptr || to == nullptr) {
            continue;
        }
        segments.emplace_back(from->box().center(), to->box().center());
    }
    return graph::count_crossings(segments);
}

// 层内顺序报告：同层节点共享同一个 x，按 x 分组还原分层，组内按 y 排序
// （y 才是布局排出来的真实层内次序，nodes() 的遍历顺序是登记顺序，不代表布局）。
// 只用于把 layout_sugiyama 的结果打印出来看，不参与布局。
std::string layer_report(const graph::Graph& graph) {
    std::map<double, std::vector<std::pair<double, std::string>>> by_x;
    for (const graph::Node& node : graph.nodes()) {
        by_x[node.box().x].emplace_back(node.box().y, node.label());
    }
    std::string out;
    std::size_t layer = 0;
    for (auto& entry : by_x) {
        std::sort(entry.second.begin(), entry.second.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        out += "  L" + std::to_string(layer++) + ":";
        for (const auto& placed : entry.second) {
            out += " ";
            out += placed.second;
        }
        out += "\n";
    }
    return out;
}

// ── 绘制 ────────────────────────────────────────────────────────────────
// 轴对齐段用制表符，斜段（路由退化时的最后手段）用对角线字符。
char32_t segment_char(Point a, Point b) noexcept {
    if (a.y == b.y) {
        return U'\u2500';  // ─
    }
    if (a.x == b.x) {
        return U'\u2502';  // │
    }
    return (b.y > a.y) == (b.x > a.x) ? U'\u2572' : U'\u2571';  // ╲ / ╱
}

// 从 from 指向 to 的臂方向（屏幕坐标 y 向下）。
graph::PortSide arm_towards(Point from, Point to) noexcept {
    if (to.x > from.x) {
        return graph::PortSide::East;
    }
    if (to.x < from.x) {
        return graph::PortSide::West;
    }
    if (to.y > from.y) {
        return graph::PortSide::South;
    }
    return graph::PortSide::North;
}

// 折点处按两条臂选角字符，比"两条直线直接叠"更像一张图。
char32_t corner_char(graph::PortSide in, graph::PortSide out) noexcept {
    const bool up = in == graph::PortSide::North || out == graph::PortSide::North;
    const bool down = in == graph::PortSide::South || out == graph::PortSide::South;
    const bool left = in == graph::PortSide::West || out == graph::PortSide::West;
    if (up && left) {
        return U'\u2518';  // ┘
    }
    if (up) {
        return U'\u2514';  // └
    }
    if (down && left) {
        return U'\u2510';  // ┐
    }
    if (down) {
        return U'\u250C';  // ┌
    }
    return left ? U'\u2500' : U'\u2502';  // 兜底：退化成直线字符
}

// 先连线（含折点），后画节点——否则连线会把标签涂掉。
void draw_edges(canvas::Canvas& canvas, const graph::Graph& graph, const Style& style) {
    for (const graph::Edge& edge : graph.edges()) {
        const std::vector<Point>& path = edge.route();
        for (std::size_t i = 1; i < path.size(); ++i) {
            canvas.draw_line(path[i - 1], path[i], segment_char(path[i - 1], path[i]), style);
        }
        for (std::size_t i = 1; i + 1 < path.size(); ++i) {
            canvas.put(path[i], corner_char(arm_towards(path[i], path[i - 1]),
                                            arm_towards(path[i], path[i + 1])),
                       style);
        }
    }
}

void draw_nodes(canvas::Canvas& canvas, const graph::Graph& graph, graph::ShapeKind kind,
                const Style& style) {
    for (const graph::Node& node : graph.nodes()) {
        graph::draw_shape(canvas, kind, node.box(), style);
        if (node.label().empty()) {
            continue;
        }
        const int label_width = util::display_width(node.label());
        const double x =
            node.box().x + std::max(0.0, (node.box().w - static_cast<double>(label_width)) / 2.0);
        const double y = node.box().y + std::floor(std::max(0.0, node.box().h - 1.0) / 2.0);
        canvas.draw_text(Point{x, y}, node.label(), style);
    }
}

void draw_arrows(canvas::Canvas& canvas, const graph::Graph& graph, const Style& style) {
    for (const graph::Edge& edge : graph.edges()) {
        const std::vector<Point>& path = edge.route();
        if (path.size() < 2 || edge.arrow() == graph::ArrowStyle::None) {
            continue;
        }
        if (edge.arrow() == graph::ArrowStyle::AtTarget || edge.arrow() == graph::ArrowStyle::Both) {
            canvas.put(path.back(), graph::arrow_head_char(
                                        graph::side_towards(path[path.size() - 2], path.back())),
                       style);
        }
        if (edge.arrow() == graph::ArrowStyle::Both) {
            canvas.put(path.front(), graph::arrow_head_char(graph::side_towards(path[1], path[0])),
                       style);
        }
    }
}

}  // namespace

int main() {
    term::Terminal terminal(term::TerminalOptions{.alt_screen = false, .hide_cursor = false});
    if (terminal.enter() != ErrorCode::Ok) {
        std::fprintf(stderr, "无法进入终端模式\n");
        return 1;
    }

    // 登记顺序 = L0: A B C、L1: D E F、L2: G H、L3: I J；边刻意逆序相连。
    graph::Graph graph;
    const graph::NodeId a = graph.add_node("A").id();
    const graph::NodeId b = graph.add_node("B").id();
    const graph::NodeId c = graph.add_node("C").id();
    const graph::NodeId d = graph.add_node("D").id();
    const graph::NodeId e = graph.add_node("E").id();
    const graph::NodeId f = graph.add_node("F").id();
    const graph::NodeId g = graph.add_node("G").id();
    const graph::NodeId h = graph.add_node("H").id();
    const graph::NodeId i = graph.add_node("I").id();
    const graph::NodeId j = graph.add_node("J").id();

    (void)graph.add_edge(a, f);
    (void)graph.add_edge(b, f);
    (void)graph.add_edge(b, e);
    (void)graph.add_edge(c, d);
    (void)graph.add_edge(f, g);
    (void)graph.add_edge(e, g);
    (void)graph.add_edge(d, h);
    (void)graph.add_edge(g, i);
    (void)graph.add_edge(h, j);

    // 列间距留够（h_gap 6）：层与层之间要容得下 Z 形绕行的竖段，太窄会让
    // 正交路由找不到通路、退化成斜线。
    graph::LayoutOptions layout;
    layout.min_width = 8.0;
    layout.min_height = 4.0;
    layout.h_gap = 6.0;
    layout.v_gap = 2.0;
    layout.margin = 1.0;

    // 基线：只分层、层内按登记顺序（不做降交叉）。
    (void)graph::layout_layered(graph, layout);
    const std::size_t crossings_before = count_center_crossings(graph);

    // 真 Sugiyama：分层 → median 双向扫描降交叉 → 坐标分配，一次调用做完三步。
    const Rect bounds = graph::layout_sugiyama(graph, layout, 4);
    const std::size_t crossings_after = count_center_crossings(graph);

    // 画布按整图包围盒取景，再钳到终端可视区（宽取 cols、高取 rows-1，留出提示符行）。
    const term::TerminalSize screen = terminal.size();
    std::uint32_t canvas_w = static_cast<std::uint32_t>(std::ceil(bounds.right())) + 2u;
    std::uint32_t canvas_h = static_cast<std::uint32_t>(std::ceil(bounds.bottom())) + 2u;
    if (screen.valid()) {
        canvas_w = std::min(canvas_w, static_cast<std::uint32_t>(screen.cols));
        canvas_h = std::min(canvas_h, static_cast<std::uint32_t>(
                                          screen.rows > 1 ? screen.rows - 1 : screen.rows));
    }
    canvas::Canvas canvas(std::max<std::uint32_t>(canvas_w, 8u), std::max<std::uint32_t>(canvas_h, 4u));

    // 第四步：正交路由（WireGrid 由 route_edges 内部把节点框登记成障碍）。
    graph::WireGrid grid(canvas.width(), canvas.height());
    graph::RouteOptions route_opts;
    route_opts.max_detour = 8;
    (void)graph::route_edges(graph, graph::RouteStyle::Orthogonal, route_opts, &grid);

    // 绘制。
    const Style node_style = Style{}.set_fg(Color::rgb(86, 197, 154));
    const Style edge_style = Style{}.set_fg(Color::ansi16(8));
    draw_edges(canvas, graph, edge_style);
    draw_nodes(canvas, graph, graph::ShapeKind::Box, node_style);
    draw_arrows(canvas, graph, edge_style);

    render::Renderer renderer(terminal.caps().color_depth);
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    if (api::write_stdout(bytes) != ErrorCode::Ok) {
        return 1;
    }

    const std::string summary =
        "\nSugiyama（graph::layout_sugiyama）：" + std::to_string(graph.node_count()) + " 节点 / " +
        std::to_string(graph.edge_count()) + " 边　交叉数 " + std::to_string(crossings_before) +
        " → " + std::to_string(crossings_after) + "（median 双向扫描降交叉）\n层内顺序：\n" +
        layer_report(graph);
    (void)api::write_stdout(summary);
    (void)api::flush_stdout();
    return terminal.leave() == ErrorCode::Ok ? 0 : 1;
}
