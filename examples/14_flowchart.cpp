// 14_flowchart.cpp —— 流程图：Graph -> 分层布局 -> 正交路由 -> 画布 -> stdout
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/style.hpp"
#include "termchart/graph/diagrams/flowchart.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    graph::Graph graph;
    const graph::NodeId start = graph.add_node("start").id();
    const graph::NodeId parse = graph.add_node("parse").id();
    const graph::NodeId check = graph.add_node("valid?").id();
    const graph::NodeId work = graph.add_node("process").id();
    const graph::NodeId fail = graph.add_node("error").id();
    const graph::NodeId done = graph.add_node("done").id();

    (void)graph.add_edge(start, parse);
    (void)graph.add_edge(parse, check);
    (void)graph.add_edge(check, work, "yes");
    (void)graph.add_edge(check, fail, "no");
    (void)graph.add_edge(work, done);
    (void)graph.add_edge(fail, done);

    canvas::Canvas canvas(64, 18);
    graph::FlowchartOptions opts;
    opts.shape = graph::ShapeKind::Box;
    opts.layout.min_width = 8.0;
    opts.layout.h_gap = 3.0;
    opts.node_style = Style{}.set_fg(Color::ansi16(6));
    opts.edge_style = Style{}.set_fg(Color::ansi16(8));
    (void)graph::render_flowchart(graph, canvas, opts);

    render::Renderer renderer;
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    const Status status = api::write_stdout(bytes);
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
