// 36_export_json.cpp —— 快照导出：Canvas → CellBuffer → JSON（写 stdout）
#include "termchart/api/print.hpp"
#include "termchart/backend/snapshot/json_backend.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    canvas::Canvas canvas(24, 3);
    canvas.draw_text(Point{0.0, 0.0}, "termchart",
                     Style{}.set_fg(Color::ansi16(2)).set_attr(Attr::Bold));
    canvas.put(Point{10.0, 0.0}, U'\u2588', Style{}.set_fg(Color::ansi16(1)));

    backend::snapshot::JsonBackend backend;
    const std::string json = backend::snapshot::render_snapshot(backend, canvas.to_buffer());

    const Status st = api::write_stdout(json);
    api::flush_stdout();
    return st == ErrorCode::Ok ? 0 : 1;
}
