// 15_state_machine.cpp —— 状态机图：分层布局 + 转移事件标签 + 起止标记
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/style.hpp"
#include "termchart/graph/diagrams/state_machine.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    graph::Graph graph;
    const graph::NodeId idle = graph.add_node("idle").id();
    const graph::NodeId running = graph.add_node("running").id();
    const graph::NodeId done = graph.add_node("done").id();
    const graph::NodeId failed = graph.add_node("error").id();

    graph.find(idle)->attributes().set("initial", "1");
    graph.find(done)->attributes().set("final", "1");

    (void)graph.add_edge(idle, running, "start");
    (void)graph.add_edge(running, done, "ok");
    (void)graph.add_edge(running, failed, "fail");
    (void)graph.add_edge(failed, idle, "reset");

    canvas::Canvas canvas(78, 18);
    graph::StateMachineOptions opts;
    opts.shape = graph::ShapeKind::Rounded;
    opts.layout.min_width = 9.0;
    opts.layout.h_gap = 12.0;  // 留出转移事件名的位置
    opts.node_style = Style{}.set_fg(Color::ansi16(6));
    opts.edge_style = Style{}.set_fg(Color::ansi16(8));
    (void)graph::render_state_machine(graph, canvas, opts);

    render::Renderer renderer;
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    const Status status = api::write_stdout(bytes);
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
