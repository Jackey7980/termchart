// 20_force_graph.cpp —— 力导向布局 + 通用节点-连线渲染
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/style.hpp"
#include "termchart/graph/diagrams/mindmap.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/force.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    graph::Graph graph;
    const graph::NodeId core = graph.add_node("core").id();
    const graph::NodeId api = graph.add_node("api").id();
    const graph::NodeId term = graph.add_node("term").id();
    const graph::NodeId scene = graph.add_node("scene").id();
    const graph::NodeId grid = graph.add_node("grid").id();
    const graph::NodeId data = graph.add_node("data").id();
    const graph::NodeId plot = graph.add_node("plot").id();

    (void)graph.add_edge(core, api);
    (void)graph.add_edge(api, term);
    (void)graph.add_edge(api, scene);
    (void)graph.add_edge(term, grid);
    (void)graph.add_edge(scene, grid);
    (void)graph.add_edge(grid, data);
    (void)graph.add_edge(data, plot);
    (void)graph.add_edge(plot, core);  // 环：力导向不需要 DAG

    canvas::Canvas canvas(78, 26);
    graph::ForceOptions force;
    force.layout.min_width = 8.0;
    force.ideal_edge_length = 8.0;
    (void)graph::layout_force(graph, force);

    graph::NodeLinkOptions opts;
    opts.route_style = graph::RouteStyle::Straight;
    opts.node_border = true;
    opts.node_style = Style{}.set_fg(Color::ansi16(2));
    opts.edge_style = Style{}.set_fg(Color::ansi16(8));
    graph::render_node_link(graph, canvas, opts);

    render::Renderer renderer;
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    const Status status = api::write_stdout(bytes);
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
