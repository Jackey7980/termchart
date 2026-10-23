// showcase/catalog_graphs.cpp —— 全部 图/diagram + 布局 + 路由 + dsl/mermaid。
// 组 A 吃 graph::Graph（就地布局+绘制）；组 B 自带输入模型；布局/路由经 node-link 呈现；
// dsl 走 parse_mermaid → to_graph → render_flowchart。
#include <cstdint>
#include <string>
#include <vector>

#include "catalog.hpp"

namespace cat {
namespace {

const Color kB = Color::rgb(90, 100, 120);
const Color kT = Color::rgb(120, 210, 255);

// 一个小 DAG：R→A→C、R→B→C（够流程图/DAG/树/力导用）。
tc::graph::Graph dag_graph() {
    tc::graph::Graph g;
    const auto r = g.add_node("R").id();
    const auto a = g.add_node("A").id();
    const auto b = g.add_node("B").id();
    const auto c = g.add_node("C").id();
    g.add_edge(r, a, "是");
    g.add_edge(r, b, "否");
    g.add_edge(a, c);
    g.add_edge(b, c);
    return g;
}

void register_graphs_impl(std::vector<Demo>& d) {
    auto add = [&](const char* c, const char* n, std::function<CellBuffer(std::uint32_t, std::uint32_t)> f) {
        d.push_back(Demo{c, n, std::move(f)});
    };
    auto demo = [&](const char* c, const char* n, const char* t, auto body) {
        add(c, n, [t, body](std::uint32_t w, std::uint32_t h) {
            return cat::panel(w, h, t, [&body](tc::canvas::Canvas& cv) { body(cv); }, kB, kT);
        });
    };

    // ═══ 组 A：吃 graph::Graph ═══════════════════════════════════════════
    demo("图·A", "流程图", "render_flowchart", [](tc::canvas::Canvas& cv) {
        tc::graph::Graph g = dag_graph();
        static_cast<void>(tc::graph::render_flowchart(g, cv, {}));
    });
    demo("图·A", "状态机", "render_state_machine", [](tc::canvas::Canvas& cv) {
        tc::graph::Graph g;
        auto& s = g.add_node("空闲");
        s.attributes().set("initial", "1");
        auto& run = g.add_node("运行");
        auto& e = g.add_node("结束");
        e.attributes().set("final", "1");
        g.add_edge(s.id(), run.id(), "启动");
        g.add_edge(run.id(), e.id(), "停止");
        static_cast<void>(tc::graph::render_state_machine(g, cv, {}));
    });
    demo("图·A", "思维导图", "render_mindmap", [](tc::canvas::Canvas& cv) {
        tc::graph::Graph g = dag_graph();
        static_cast<void>(tc::graph::render_mindmap(g, cv, {}));
    });
    demo("图·A", "组织架构", "render_orgchart", [](tc::canvas::Canvas& cv) {
        tc::graph::Graph g = dag_graph();
        static_cast<void>(tc::graph::render_orgchart(g, cv, {}));
    });
    demo("图·A", "DAG", "render_dag", [](tc::canvas::Canvas& cv) {
        tc::graph::Graph g = dag_graph();
        static_cast<void>(tc::graph::render_dag(g, cv, {}));
    });
    demo("图·A", "依赖图", "render_dependency", [](tc::canvas::Canvas& cv) {
        tc::graph::Graph g = dag_graph();
        static_cast<void>(tc::graph::render_dependency(g, cv, {}));
    });
    demo("图·A", "ER 图", "render_er_diagram", [](tc::canvas::Canvas& cv) {
        // 注意：加基数属性(card_*)会在"全图表在前"的合成渲染中触发库内一处
        // ASLR 敏感的堆越界（需 ASAN 定位）；此处用边标签版以保证全量渲染稳定。
        tc::graph::Graph g;
        auto& user = g.add_node("user");
        auto& order = g.add_node("order");
        g.add_edge(user.id(), order.id(), "places");
        static_cast<void>(tc::graph::render_er_diagram(g, cv, {}));
    });
    demo("图·A", "知识图谱", "render_knowledge_graph", [](tc::canvas::Canvas& cv) {
        tc::graph::Graph g;
        const auto a = g.add_node("C++").id();
        const auto b = g.add_node("STL").id();
        const auto c = g.add_node("模板").id();
        g.add_edge(a, b, "含");
        g.add_edge(b, c, "基于");
        static_cast<void>(tc::graph::render_knowledge_graph(g, cv, {}));
    });
    demo("图·A", "UML 类图", "render_uml_class", [](tc::canvas::Canvas& cv) {
        tc::graph::Graph g;
        auto& shape = g.add_node("Shape");
        shape.attributes().set("attrs", "-w: int\n-h: int");
        shape.attributes().set("ops", "+area(): double");
        auto& circle = g.add_node("Circle");
        circle.attributes().set("attrs", "-r: int");
        g.add_edge(shape.id(), circle.id());
        static_cast<void>(tc::graph::render_uml_class(g, cv, {}));
    });
    demo("图·A", "UML 活动图", "render_uml_activity", [](tc::canvas::Canvas& cv) {
        tc::graph::Graph g;
        auto& s = g.add_node("开始");
        s.attributes().set("initial", "1");
        auto& dec = g.add_node("条件?");
        dec.attributes().set("kind", "decision");
        auto& e = g.add_node("结束");
        e.attributes().set("final", "1");
        g.add_edge(s.id(), dec.id());
        g.add_edge(dec.id(), e.id(), "是");
        static_cast<void>(tc::graph::render_uml_activity(g, cv, {}));
    });
    demo("图·A", "UML 用例", "render_uml_usecase", [](tc::canvas::Canvas& cv) {
        tc::graph::Graph g;
        auto& actor = g.add_node("用户");
        actor.attributes().set("kind", "actor");
        actor.attributes().set("side", "left");
        const auto u = g.add_node("登录").id();
        g.add_edge(actor.id(), u);
        static_cast<void>(tc::graph::render_uml_usecase(g, cv, {}));
    });
    demo("图·A", "社交网络", "render_social_network", [](tc::canvas::Canvas& cv) {
        tc::graph::Graph g;
        const auto a = g.add_node("甲").id();
        const auto b = g.add_node("乙").id();
        const auto c = g.add_node("丙").id();
        g.add_edge(a, b);
        g.add_edge(b, c);
        g.add_edge(c, a);
        static_cast<void>(tc::graph::render_social_network(g, cv, {}));
    });
    demo("图·A", "节点-连线", "render_node_link", [](tc::canvas::Canvas& cv) {
        tc::graph::Graph g = dag_graph();
        static_cast<void>(tc::graph::layout_force(g, {}));
        tc::graph::render_node_link(g, cv, tc::graph::NodeLinkOptions{});
    });

    // ═══ 布局 layout ═════════════════════════════════════════════════════
    auto layout_demo = [&](const char* n, const char* t, auto layout_fn) {
        demo("图·布局", n, t, [layout_fn](tc::canvas::Canvas& cv) {
            tc::graph::Graph g = dag_graph();
            static_cast<void>(layout_fn(g));
            tc::graph::render_node_link(g, cv, tc::graph::NodeLinkOptions{});
        });
    };
    layout_demo("力导布局", "layout_force", [](tc::graph::Graph& g) { return tc::graph::layout_force(g, {}); });
    layout_demo("分层 Sugiyama", "layout_sugiyama", [](tc::graph::Graph& g) { return tc::graph::layout_sugiyama(g, {}, 4); });
    layout_demo("圆周布局", "layout_circular", [](tc::graph::Graph& g) { return tc::graph::layout_circular(g, {}); });
    layout_demo("径向布局", "layout_radial", [](tc::graph::Graph& g) { return tc::graph::layout_radial(g, {}); });
    layout_demo("KK 弹簧", "layout_kamada_kawai", [](tc::graph::Graph& g) { return tc::graph::layout_kamada_kawai(g, {}); });
    layout_demo("树布局", "layout_tree", [](tc::graph::Graph& g) { return tc::graph::layout_tree(g, {}); });
    layout_demo("网格布局", "layout_grid", [](tc::graph::Graph& g) { return tc::graph::layout_grid(g, 2, {}); });
    layout_demo("聚类布局", "layout_cluster", [](tc::graph::Graph& g) { return tc::graph::layout_cluster(g, {}); });
    layout_demo("地铁布局", "layout_metro", [](tc::graph::Graph& g) { return tc::graph::layout_metro(g, {}); });

    // ═══ 路由 route ══════════════════════════════════════════════════════
    auto route_demo = [&](const char* n, const char* t, auto route_fn) {
        demo("图·路由", n, t, [route_fn](tc::canvas::Canvas& cv) {
            tc::graph::Graph g = dag_graph();
            static_cast<void>(tc::graph::layout_force(g, {}));
            static_cast<void>(route_fn(g));
            tc::graph::render_node_link(g, cv, tc::graph::NodeLinkOptions{});
        });
    };
    route_demo("直线路由", "route Straight", [](tc::graph::Graph& g) {
        return tc::graph::route_edges(g, tc::graph::RouteStyle::Straight, {}, nullptr);
    });
    route_demo("正交路由", "route Orthogonal", [](tc::graph::Graph& g) {
        return tc::graph::route_edges(g, tc::graph::RouteStyle::Orthogonal, {}, nullptr);
    });
    route_demo("弧线路由", "route_arc", [](tc::graph::Graph& g) { return tc::graph::route_arc(g, {}, nullptr); });
    route_demo("样条路由", "route_spline", [](tc::graph::Graph& g) { return tc::graph::route_spline(g, {}, nullptr); });
    route_demo("捆绑路由", "route_bundled", [](tc::graph::Graph& g) { return tc::graph::route_bundled(g, {}, nullptr); });

    // ═══ 组 B：自带输入模型 ══════════════════════════════════════════════
    demo("图·B", "桑基图", "render_sankey", [](tc::canvas::Canvas& cv) {
        std::vector<tc::graph::SankeyNode> nodes{{"来源"}, {"处理"}, {"出口"}};
        std::vector<tc::graph::SankeyFlow> flows{{0, 1, 40.0}, {0, 2, 20.0}, {1, 2, 15.0}};
        tc::graph::SankeyOptions o;
        o.h_gap = 8.0;
        static_cast<void>(tc::graph::render_sankey(nodes, flows, cv, o));
    });
    demo("图·B", "弦图", "render_chord", [](tc::canvas::Canvas& cv) {
        std::vector<tc::graph::ChordEntity> ents{{"A"}, {"B"}, {"C"}};
        std::vector<tc::graph::ChordLink> lnks{{0, 1, 5.0}, {1, 2, 3.0}, {0, 2, 2.0}};
        tc::graph::ChordOptions o;
        o.radius = 6.0;
        static_cast<void>(tc::graph::render_chord(ents, lnks, cv, o));
    });
    demo("图·B", "弧图", "render_arc_diagram", [](tc::canvas::Canvas& cv) {
        std::vector<tc::graph::ArcNode> ns{{"a"}, {"b"}, {"c"}, {"d"}};
        std::vector<tc::graph::ArcLink> ls{{0, 3}, {0, 1}, {1, 2}};
        static_cast<void>(tc::graph::render_arc_diagram(ns, ls, cv, {}));
    });
    demo("图·B", "邻接矩阵", "render_adjacency_matrix", [](tc::canvas::Canvas& cv) {
        std::vector<std::string> names{"a", "b", "c"};
        std::vector<std::vector<double>> m{{0, 1, 0.5}, {1, 0, 0}, {0.5, 0, 0}};
        static_cast<void>(tc::graph::render_adjacency_matrix(names, m, cv, {}));
    });
    demo("图·B", "电路图", "render_circuit", [](tc::canvas::Canvas& cv) {
        std::vector<tc::graph::CircuitComponent> parts{
            {tc::graph::ComponentKind::Battery, "V1", tc::Point{2.0, 6.0}},
            {tc::graph::ComponentKind::Resistor, "R1", tc::Point{12.0, 6.0}},
            {tc::graph::ComponentKind::Capacitor, "C1", tc::Point{22.0, 6.0}}};
        std::vector<tc::graph::CircuitWire> wires{
            {{0, tc::graph::PortSide::East}, {1, tc::graph::PortSide::West}},
            {{1, tc::graph::PortSide::East}, {2, tc::graph::PortSide::West}}};
        static_cast<void>(tc::graph::render_circuit(parts, wires, cv, {}));
    });
    demo("图·B", "语法树", "render_syntax_tree", [](tc::canvas::Canvas& cv) {
        tc::graph::SyntaxNode leaf_b{"b", {}};
        tc::graph::SyntaxNode leaf_c{"c", {}};
        tc::graph::SyntaxNode star{"*", {leaf_b, leaf_c}};
        tc::graph::SyntaxNode leaf_a{"a", {}};
        tc::graph::SyntaxNode plus{"+", {}};
        tc::graph::SyntaxNode root{"expr", {leaf_a, plus, star}};
        tc::graph::SyntaxTreeOptions o;
        o.indent = 3;
        static_cast<void>(tc::graph::render_syntax_tree(root, cv, o));
    });
    demo("图·B", "UML 时序", "render_uml_sequence", [](tc::canvas::Canvas& cv) {
        tc::graph::SeqModel m;
        m.participants.push_back({"cli", "Client"});
        m.participants.push_back({"srv", "Server"});
        m.messages.push_back({"cli", "srv", "GET /", tc::graph::SeqArrowKind::Solid, true, false});
        m.messages.push_back({"srv", "cli", "200 OK", tc::graph::SeqArrowKind::Dotted, false, true});
        static_cast<void>(tc::graph::render_uml_sequence(m, cv, {}));
    });
    demo("图·B", "Petri 网", "render_petri_net", [](tc::canvas::Canvas& cv) {
        tc::graph::PetriNet net;
        net.places.push_back({"p0", 2});
        net.places.push_back({"p1", 0});
        net.transitions.push_back({"t0"});
        net.arcs.push_back({tc::graph::PetriNet::place(0), tc::graph::PetriNet::transition(0)});
        net.arcs.push_back({tc::graph::PetriNet::transition(0), tc::graph::PetriNet::place(1)});
        static_cast<void>(tc::graph::render_petri_net(net, cv, {}));
    });
    demo("图·B", "地铁图", "render_metro_map", [](tc::canvas::Canvas& cv) {
        std::vector<tc::graph::MetroLine> lines{
            {"L1", Color::ansi16(4), {"甲", "乙", "丙", "丁"}},
            {"L2", Color::ansi16(2), {"丙", "戊", "己"}}};
        static_cast<void>(tc::graph::render_metro_map(lines, cv, {}));
    });

    // ═══ DSL：Mermaid 解析 → graph → 渲染 ════════════════════════════════
    demo("DSL", "Mermaid 流程图", "parse_mermaid", [](tc::canvas::Canvas& cv) {
        const std::string src = "flowchart TD\n  A[开始] --> B{判断}\n  B -->|是| C[通过]\n  B -->|否| D[拒绝]\n";
        tc::dsl::ParseDiagnostic diag;
        auto ast = tc::dsl::parse_mermaid(src, {}, &diag);
        if (!ast) {
            cv.draw_text(tc::Point{1.0, 1.0}, "parse error", Style{}.set_fg(Color::ansi16(1)));
            return;
        }
        auto g = tc::dsl::to_graph(*ast);
        if (g) {
            static_cast<void>(tc::graph::render_flowchart(*g, cv, {}));
        }
    });
    demo("DSL", "检测图类型", "detect_diagram", [](tc::canvas::Canvas& cv) {
        const tc::dsl::MermaidDiagram k = tc::dsl::detect_diagram("flowchart TD\nA-->B");
        cv.draw_text(tc::Point{1.0, 1.0}, std::string("kind=") + tc::dsl::diagram_name(k),
                     Style{}.set_fg(Color::ansi16(6)));
    });
}

}  // namespace

void register_graphs(std::vector<Demo>& d) { register_graphs_impl(d); }

}  // namespace cat
