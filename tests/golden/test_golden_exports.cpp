// test_golden_exports.cpp —— text / svg / json 三类导出快照（ROADMAP 阶段 3 出口条件）
//
// 与 ansi_props 不同，这三类直接比对**序列化字节**（导出格式本身就是契约）。
// 快照由 `python tools/gen_golden.py` 生成；**禁止手改**。
#include <doctest.h>

#include <string>

#include "golden/golden_runner.hpp"
#include "termchart/backend/snapshot/json_backend.hpp"
#include "termchart/backend/snapshot/text_backend.hpp"
#include "termchart/backend/vector/svg_backend.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/style.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"

namespace {

using namespace termchart;

// 确定性输入：固定画布 + 固定样式（含宽字符与其续接格）。
CellBuffer fixed_buffer() {
    CellBuffer buffer(6, 2);
    buffer.at(0, 0) = Cell{U'h', Style{}.set_fg(Color::ansi16(2))};
    buffer.at(1, 0) = Cell{U'i', Style{}.set_fg(Color::ansi16(2))};
    buffer.at(0, 1) = Cell{U'\u4F60', Style{}};  // 你
    buffer.at(1, 1) = Cell{kContinuationCp, Style{}};
    return buffer;
}

scene::Scene fixed_scene() {
    scene::Scene scene;
    scene.push(scene::MoveTo{Point{0.0, 0.0}});
    scene.push(scene::LineTo{Point{10.0, 4.0}});
    scene.push(scene::StrokeOp{Color::rgb(0x5B, 0x8F, 0xF9), 1.5});
    scene.push(scene::TextOp{Point{1.0, 1.0}, "hi<&>", Style{}.set_fg(Color::rgb(0, 0, 0))});
    return scene;
}

}  // namespace

TEST_CASE("golden: text 导出快照") {
    backend::snapshot::TextBackend backend;
    golden::check_or_update("snapshot_text", "basic",
                            backend::snapshot::render_snapshot(backend, fixed_buffer()));
}

TEST_CASE("golden: json 导出快照") {
    backend::snapshot::JsonBackend backend;
    golden::check_or_update("snapshot_json", "basic",
                            backend::snapshot::render_snapshot(backend, fixed_buffer()));
}

TEST_CASE("golden: svg 导出快照") {
    backend::vector::SvgBackend backend;
    backend.set_options(backend::vector::VectorOptions{.scale = 10.0, .autofit = true});
    golden::check_or_update("vector_svg", "basic",
                            backend::vector::render_vector(backend, fixed_scene()));
}
