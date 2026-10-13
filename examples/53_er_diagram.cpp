// 53_er_diagram.cpp —— ER 图：实体表框 + 鸭掌记号基数 + 关系名标签（规格示例）
//
// 消费 graph/diagrams/er_diagram.hpp：render_er_diagram。三个实体（user /
// order / item），两条 1:N 关系；fields 与基数走 Node/Edge 的 attributes
// 键值袋（模型约定见头文件）。
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/style.hpp"
#include "termchart/graph/diagrams/er_diagram.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    graph::Graph graph;
    const graph::NodeId user = graph.add_node("user").id();
    const graph::NodeId order = graph.add_node("order").id();
    const graph::NodeId item = graph.add_node("item").id();

    graph.find(user)->attributes().set("fields", "id: pk\nname\nemail");
    graph.find(order)->attributes().set("fields", "id: pk\nuser_id: fk\ntotal");
    graph.find(item)->attributes().set("fields", "id: pk\norder_id: fk\nname");

    graph::Edge* places = graph.add_edge(user, order, "places");
    places->attributes().set("card_from", "1");
    places->attributes().set("card_to", "N");
    graph::Edge* contains = graph.add_edge(order, item, "contains");
    contains->attributes().set("card_from", "1");
    contains->attributes().set("card_to", "N");

    graph::ErDiagramOptions opts;
    opts.layout.min_width = 10.0;
    // h_gap 给足宽度：关系名标签"放不下宁可不画"（见 er_diagram.hpp 头注），
    // 间距太小会连标签一起省掉。
    opts.layout.h_gap = 12.0;
    opts.entity_style = Style{}.set_fg(Color::ansi16(8));
    opts.name_style = Style{}.set_fg(Color::ansi16(7));
    opts.field_style = Style{}.set_fg(Color::ansi16(2));
    opts.edge_style = Style{}.set_fg(Color::ansi16(6));

    canvas::Canvas canvas(92, 20);
    (void)graph::render_er_diagram(graph, canvas, opts);

    render::Renderer renderer;
    const Status status = api::write_stdout(renderer.render_full(canvas.to_buffer()) + "\n");
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
