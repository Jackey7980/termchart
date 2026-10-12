// 18_uml_sequence.cpp —— UML 时序图（规格示例）
//
// client -> server -> library 的两次同步往返（调用走实线实心箭头，
// 返回走虚线实心箭头），外加一条跨 server/library 的 Note；
// 消息带 activate_target / deactivate_source，生命线上出现激活条。
// 直接喂 graph 本层的 SeqModel（dsl 的 AstSequence 由上层适配，不经此示例）。
// 输出自包含（固定画布，不依赖终端尺寸）。
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/style.hpp"
#include "termchart/graph/diagrams/uml_sequence.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

graph::SeqModel make_model() {
    graph::SeqModel model;
    model.participants.push_back({"client", "client"});
    model.participants.push_back({"server", "server"});
    model.participants.push_back({"library", "library"});

    model.messages.push_back(
        {"client", "server", "GET /api/items", graph::SeqArrowKind::Solid,
         /*activate_target=*/true, /*deactivate_source=*/false});
    model.messages.push_back(
        {"server", "library", "query(sql)", graph::SeqArrowKind::Solid,
         /*activate_target=*/true, /*deactivate_source=*/false});
    model.messages.push_back(
        {"library", "server", "rows", graph::SeqArrowKind::Dotted,
         /*activate_target=*/false, /*deactivate_source=*/true});
    model.messages.push_back(
        {"server", "client", "200 OK", graph::SeqArrowKind::Dotted,
         /*activate_target=*/false, /*deactivate_source=*/true});

    graph::SeqNote note;
    note.placement = graph::SeqNote::Placement::Over;
    note.anchor = "server";
    note.anchor_b = "library";
    note.text = "index scan";
    note.after = 2;  // 出现在 "query(sql)" 之后
    model.notes.push_back(note);
    return model;
}

}  // namespace

int main() {
    const graph::SeqModel model = make_model();

    canvas::Canvas canvas(48, 16);
    graph::UmlSequenceOptions opts;
    opts.header_style = Style{}.set_fg(Color::ansi16(6));
    opts.lifeline_style = Style{}.set_fg(Color::ansi16(8));
    opts.message_style = Style{}.set_fg(Color::ansi16(7));
    opts.note_style = Style{}.set_fg(Color::ansi16(3));
    (void)graph::render_uml_sequence(model, canvas, opts);

    render::Renderer renderer;
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    const Status status = api::write_stdout(bytes);
    (void)api::write_stdout("\n");
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
