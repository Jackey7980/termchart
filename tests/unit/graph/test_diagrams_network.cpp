// test_diagrams_network.cpp —— 五种网络图渲染
// （知识图谱 / 社交网络 / Petri 网 / 地铁线路图 / 桑基图）
#include <doctest.h>

#include <cstddef>
#include <cstdint>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/style.hpp"
#include "termchart/graph/diagrams/knowledge_graph.hpp"
#include "termchart/graph/diagrams/metro_map.hpp"
#include "termchart/graph/diagrams/petri_net.hpp"
#include "termchart/graph/diagrams/sankey.hpp"
#include "termchart/graph/diagrams/social_network.hpp"
#include "termchart/graph/graph.hpp"

namespace {

using namespace termchart;
using namespace termchart::graph;

bool has_char(const canvas::Canvas& canvas, char32_t cp) {
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            if (canvas.at(x, y).cp == cp) {
                return true;
            }
        }
    }
    return false;
}

// 箭头四个方向字符。
bool has_arrow(const canvas::Canvas& canvas) {
    return has_char(canvas, U'\u25B6') || has_char(canvas, U'\u25C0') ||
           has_char(canvas, U'\u25B2') || has_char(canvas, U'\u25BC');
}

// 某一行上是否连续出现给定 ASCII 文本（确认标签落到了画布上）。
bool row_contains(const canvas::Canvas& canvas, const std::string& text) {
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x + text.size() <= canvas.width(); ++x) {
            bool match = true;
            for (std::size_t k = 0; k < text.size(); ++k) {
                if (canvas.at(x + static_cast<std::uint32_t>(k), y).cp !=
                    static_cast<char32_t>(text[k])) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return true;
            }
        }
    }
    return false;
}

// 某列上指定码点的格数（桑基图条高回归用）。
std::size_t count_in_column(const canvas::Canvas& canvas, std::uint32_t x,
                            char32_t cp) {
    std::size_t count = 0;
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        if (canvas.at(x, y).cp == cp) {
            ++count;
        }
    }
    return count;
}

// 是否存在前景为指定 16 色索引的非空格（地铁线色回归用）。
bool has_fg_index(const canvas::Canvas& canvas, std::uint8_t index) {
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            const Cell& cell = canvas.at(x, y);
            if (cell.cp != U' ' && cell.style.fg().kind() == ColorKind::Ansi16 &&
                cell.style.fg().index() == index) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

TEST_CASE("diagrams/knowledge_graph: 力导向布局 + 边框盒子 + 默认无箭头") {
    Graph graph;
    const NodeId a = graph.add_node("alpha").id();
    const NodeId b = graph.add_node("beta").id();
    const NodeId c = graph.add_node("gamma").id();
    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, c);
    (void)graph.add_edge(c, a);

    canvas::Canvas canvas(70, 30);
    KnowledgeGraphOptions opts;
    opts.force.layout.min_width = 8.0;
    const Rect bounds = render_knowledge_graph(graph, canvas, opts);
    CHECK(bounds.w > 0.0);
    CHECK(has_char(canvas, U'\u250C'));  // 直角框
    CHECK(row_contains(canvas, "alpha"));
    CHECK(row_contains(canvas, "gamma"));
    CHECK_FALSE(has_arrow(canvas));  // 默认不带箭头

    // 节点两两不重叠。
    for (std::size_t i = 0; i < graph.node_count(); ++i) {
        for (std::size_t j = i + 1; j < graph.node_count(); ++j) {
            const Rect p = graph.nodes()[i].box();
            const Rect q = graph.nodes()[j].box();
            const bool overlap = p.x < q.right() && q.x < p.right() &&
                                 p.y < q.bottom() && q.y < p.bottom();
            CHECK_FALSE(overlap);
        }
    }
}

TEST_CASE("diagrams/knowledge_graph: 开箭头后可见") {
    Graph graph;
    const NodeId a = graph.add_node("a").id();
    const NodeId b = graph.add_node("b").id();
    (void)graph.add_edge(a, b);

    canvas::Canvas canvas(60, 20);
    KnowledgeGraphOptions opts;
    opts.arrows = true;
    (void)render_knowledge_graph(graph, canvas, opts);
    CHECK(has_arrow(canvas));
}

TEST_CASE("diagrams/social_network: 环形摆位 + 无箭头 + 不重叠") {
    Graph graph;
    graph.add_node("alice");
    graph.add_node("bob");
    graph.add_node("carol");
    graph.add_node("dave");
    const NodeId alice = graph.nodes()[0].id();
    const NodeId bob = graph.nodes()[1].id();
    const NodeId carol = graph.nodes()[2].id();
    const NodeId dave = graph.nodes()[3].id();
    (void)graph.add_edge(alice, bob);
    (void)graph.add_edge(bob, carol);
    (void)graph.add_edge(carol, dave);
    (void)graph.add_edge(dave, alice);

    canvas::Canvas canvas(60, 22);
    const Rect bounds = render_social_network(graph, canvas, SocialNetworkOptions{});
    CHECK(bounds.w > 0.0);
    CHECK(has_char(canvas, U'\u250C'));
    CHECK(row_contains(canvas, "alice"));
    CHECK(row_contains(canvas, "carol"));
    CHECK_FALSE(has_arrow(canvas));

    for (std::size_t i = 0; i < graph.node_count(); ++i) {
        for (std::size_t j = i + 1; j < graph.node_count(); ++j) {
            const Rect p = graph.nodes()[i].box();
            const Rect q = graph.nodes()[j].box();
            const bool overlap = p.x < q.right() && q.x < p.right() &&
                                 p.y < q.bottom() && q.y < p.bottom();
            CHECK_FALSE(overlap);
        }
    }
}

TEST_CASE("diagrams/social_network: 单节点退化为角落摆位") {
    Graph graph;
    graph.add_node("solo");
    canvas::Canvas canvas(20, 10);
    const Rect bounds = render_social_network(graph, canvas, SocialNetworkOptions{});
    CHECK(bounds.w > 0.0);
    CHECK(row_contains(canvas, "solo"));
}

TEST_CASE("diagrams/petri_net: 库所椭圆 + token + 变迁 + 弧箭头") {
    PetriNet net;
    net.places.push_back({"input", 3});
    net.places.push_back({"done", 0});
    net.transitions.push_back({"work"});
    net.transitions.push_back({"reset"});
    net.arcs.push_back({PetriNet::place(0), PetriNet::transition(0)});
    net.arcs.push_back({PetriNet::transition(0), PetriNet::place(1)});
    net.arcs.push_back({PetriNet::place(1), PetriNet::transition(1)});
    net.arcs.push_back({PetriNet::transition(1), PetriNet::place(0)});

    canvas::Canvas canvas(60, 15);
    const Rect bounds = render_petri_net(net, canvas, PetriNetOptions{});
    CHECK(bounds.w > 0.0);
    CHECK(has_char(canvas, U'\u25CF'));  // ● token
    CHECK(has_char(canvas, U'\u2502'));  // 变迁/椭圆的竖边
    CHECK(has_arrow(canvas));            // 弧带箭头
    CHECK(row_contains(canvas, "input"));
    CHECK(row_contains(canvas, "work"));
}

TEST_CASE("diagrams/petri_net: token 超上限改画数字") {
    PetriNet net;
    net.places.push_back({"p", 9});
    PetriNetOptions opts;
    opts.max_token_dots = 5;
    canvas::Canvas canvas(30, 10);
    (void)render_petri_net(net, canvas, opts);
    CHECK_FALSE(has_char(canvas, U'\u25CF'));
    CHECK(row_contains(canvas, "9"));
}

TEST_CASE("diagrams/petri_net: 端点越界的弧被跳过") {
    PetriNet net;
    net.places.push_back({"p", 0});
    net.transitions.push_back({"t"});
    net.arcs.push_back({PetriNet::place(0), PetriNet::transition(5)});
    canvas::Canvas canvas(30, 10);
    const Rect bounds = render_petri_net(net, canvas, PetriNetOptions{});
    CHECK(bounds.w > 0.0);
    CHECK_FALSE(has_arrow(canvas));
}

TEST_CASE("diagrams/metro_map: 换乘站合并 + 45° 走线 + 线色") {
    std::vector<MetroLine> lines;
    MetroLine red;
    red.name = "red";
    red.color = Color::ansi16(1);
    red.stations = {"A", "B", "C"};
    MetroLine blue;
    blue.name = "blue";
    blue.color = Color::ansi16(4);
    blue.stations = {"C", "D"};
    lines.push_back(red);
    lines.push_back(blue);

    canvas::Canvas canvas(44, 26);
    MetroMapOptions opts;
    opts.metro.layout.min_width = 6.0;
    opts.metro.zigzag = 1;
    const Rect bounds = render_metro_map(lines, canvas, opts);
    CHECK(bounds.w > 0.0);
    CHECK(row_contains(canvas, "A"));
    CHECK(row_contains(canvas, "B"));
    CHECK(row_contains(canvas, "C"));
    CHECK(row_contains(canvas, "D"));
    CHECK_FALSE(has_arrow(canvas));
    // 45° 走线：至少出现一种斜线字符。
    CHECK((has_char(canvas, U'\u2571') || has_char(canvas, U'\u2572')));
    // 线色：红 / 蓝两条线的走线格保留各自前景色。
    CHECK(has_fg_index(canvas, 1));
    CHECK(has_fg_index(canvas, 4));
}

TEST_CASE("diagrams/metro_map: 未指定线色走兜底样式") {
    std::vector<MetroLine> lines;
    MetroLine gray;
    gray.name = "gray";
    gray.stations = {"A", "B"};
    lines.push_back(gray);

    canvas::Canvas canvas(30, 16);
    MetroMapOptions opts;
    opts.metro.layout.min_width = 6.0;
    opts.metro.zigzag = 1;
    Style fallback;
    fallback.set_fg(Color::ansi16(7));
    opts.fallback_line_style = fallback;
    (void)render_metro_map(lines, canvas, opts);
    CHECK(has_fg_index(canvas, 7));
}

TEST_CASE("diagrams/sankey: 条高按流量、丝带过渡、标签落位") {
    std::vector<SankeyNode> nodes = {{"src"}, {"mid"}, {"dst"}};
    std::vector<SankeyFlow> flows = {
        {0, 1, 3.0},
        {1, 2, 1.0},
    };

    canvas::Canvas canvas(30, 16);
    const Rect bounds = render_sankey(nodes, flows, canvas, SankeyOptions{});
    CHECK(bounds.w > 0.0);
    CHECK(has_char(canvas, U'\u2588'));  // █ 节点条
    CHECK(has_char(canvas, U'\u2591'));  // ░ 丝带
    CHECK(row_contains(canvas, "src"));
    CHECK(row_contains(canvas, "dst"));
    // 条高 ∝ 流量：src（出流 3）的条高于 dst（入流 1）。三阶段列 x = 1/8/15。
    const std::size_t src_h = count_in_column(canvas, 1, U'\u2588');
    const std::size_t dst_h = count_in_column(canvas, 15, U'\u2588');
    CHECK(src_h > dst_h);
    CHECK(dst_h >= 1);
}

TEST_CASE("diagrams/sankey: 无效流被跳过，空输入返回空包围盒") {
    std::vector<SankeyNode> nodes = {{"a"}, {"b"}};
    std::vector<SankeyFlow> flows = {
        {0, 5, 2.0},   // 端点越界
        {0, 0, 2.0},   // 自流
        {0, 1, -1.0},  // value <= 0
    };
    canvas::Canvas canvas(30, 12);
    const Rect bounds = render_sankey(nodes, flows, canvas, SankeyOptions{});
    CHECK(bounds.w > 0.0);
    CHECK(has_char(canvas, U'\u2588'));
    CHECK_FALSE(has_char(canvas, U'\u2591'));  // 没有有效流就没有丝带
    CHECK(row_contains(canvas, "a"));
    CHECK(row_contains(canvas, "b"));

    std::vector<SankeyNode> empty;
    canvas::Canvas blank(10, 6);
    const Rect nothing = render_sankey(empty, flows, blank, SankeyOptions{});
    CHECK(nothing.empty());
}

TEST_CASE("diagrams/network: 空图各入口都返回空包围盒") {
    Graph graph;
    canvas::Canvas canvas(20, 8);
    CHECK(render_knowledge_graph(graph, canvas, KnowledgeGraphOptions{}).empty());
    CHECK(render_social_network(graph, canvas, SocialNetworkOptions{}).empty());

    std::vector<MetroLine> no_lines;
    CHECK(render_metro_map(no_lines, canvas, MetroMapOptions{}).empty());

    PetriNet no_net;
    CHECK(render_petri_net(no_net, canvas, PetriNetOptions{}).empty());
}
