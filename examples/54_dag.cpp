// 54_dag.cpp —— DAG：无环走分层+正交路由；带环输入走退化路径（规格示例）
//
// 消费 graph/diagrams/dag.hpp：is_acyclic + render_dag。图里故意放一条
// c→a 回边构成环：is_acyclic() 返回 false（写在画布标题里），render_dag
// 不报错不丢边，退化为普通分层布局并照常画出回边与箭头。
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/graph/diagrams/dag.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    graph::Graph graph;
    const graph::NodeId a = graph.add_node("a").id();
    const graph::NodeId b = graph.add_node("b").id();
    const graph::NodeId c = graph.add_node("c").id();
    const graph::NodeId d = graph.add_node("d").id();
    const graph::NodeId e = graph.add_node("e").id();

    (void)graph.add_edge(a, b);
    (void)graph.add_edge(b, c);
    (void)graph.add_edge(a, c);
    (void)graph.add_edge(c, d);
    (void)graph.add_edge(c, e);
    (void)graph.add_edge(c, a, "back");  // 回边：人为引入环

    const bool acyclic = graph::is_acyclic(graph);

    graph::DagOptions opts;
    opts.layout.min_width = 8.0;
    opts.layout.h_gap = 4.0;
    opts.shape = graph::ShapeKind::Box;
    opts.node_style = Style{}.set_fg(Color::ansi16(6));
    opts.edge_style = Style{}.set_fg(Color::ansi16(8));

    canvas::Canvas canvas(76, 20);
    const char* headline =
        acyclic ? "dag: acyclic input"
                : "dag: cyclic input -> degraded layered layout (back edge kept)";
    canvas.draw_text(Point{0.0, 0.0}, headline, Style{}.set_fg(Color::ansi16(7)));
    (void)graph::render_dag(graph, canvas, opts);

    render::Renderer renderer;
    const Status status = api::write_stdout(renderer.render_full(canvas.to_buffer()) + "\n");
    api::flush_stdout();
    return (status == ErrorCode::Ok && !acyclic) ? 0 : 1;
}
