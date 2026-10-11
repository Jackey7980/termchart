// 16_mindmap.cpp —— 思维导图：树布局（根居中、分支分列左右）+ 无箭头连线
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/style.hpp"
#include "termchart/graph/diagrams/mindmap.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    graph::Graph graph;
    const graph::NodeId center = graph.add_node("termchart").id();
    const graph::NodeId render = graph.add_node("render").id();
    const graph::NodeId layout = graph.add_node("layout").id();
    const graph::NodeId chart = graph.add_node("chart").id();
    const graph::NodeId graph_mod = graph.add_node("graph").id();
    const graph::NodeId export_mod = graph.add_node("backend").id();
    const graph::NodeId term = graph.add_node("term").id();

    (void)graph.add_edge(center, render);
    (void)graph.add_edge(center, layout);
    (void)graph.add_edge(center, chart);
    (void)graph.add_edge(center, graph_mod);
    (void)graph.add_edge(center, export_mod);
    (void)graph.add_edge(center, term);
    (void)graph.add_edge(render, chart);

    canvas::Canvas canvas(84, 18);
    graph::MindmapOptions opts;
    opts.topic_style = Style{}.set_fg(Color::ansi16(5));
    opts.branch_style = Style{}.set_fg(Color::ansi16(8));
    (void)graph::render_mindmap(graph, canvas, opts);

    render::Renderer renderer;
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    const Status status = api::write_stdout(bytes);
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
