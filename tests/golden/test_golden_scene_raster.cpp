// test_golden_scene_raster.cpp —— Scene → Canvas 的终端外观快照（R12 收口锁）
//
// 为什么需要它：`render_scene` 从"kStrokeChar = '*' 占位"改成"描边走盲文 2×4 子像素、
// 填充走半块 1×2 实心"之后，外观成了要长期守住的契约。用 **ANSI 快照**而不是纯文本：
// 颜色也在字节里，子像素取色回退会被当场发现。
// 快照由 `python tools/gen_golden.py` 生成；禁止手改。
#include <doctest.h>

#include <string>

#include "golden/golden_runner.hpp"
#include "termchart/backend/snapshot/ansi_backend.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/scene_renderer.hpp"
#include "termchart/core/style.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"

namespace {

using namespace termchart;

// 固定 Scene，覆盖四条取值路径：折线描边、圆弧描边、填充矩形、文本。
scene::Scene fixed_scene() {
    scene::Scene scene;
    scene.push(scene::StrokeOp{Color::ansi16(2), 1.0});
    scene.push(scene::MoveTo{Point{0.0, 0.0}});
    scene.push(scene::LineTo{Point{10.0, 5.0}});
    scene.push(scene::LineTo{Point{19.0, 1.0}});
    scene.push(scene::StrokeOp{Color::ansi16(4), 1.0});
    scene.push(scene::ArcOp{Point{22.0, 4.0}, 3.0, 0.0, 6.283185307179586});
    scene.push(scene::FillOp{Color::ansi16(1)});
    scene.push(scene::RectOp{Rect{2.0, 7.0, 6.0, 3.0}});
    scene.push(scene::TextOp{Point{12.0, 7.0}, "hi", Style{}.set_fg(Color::ansi16(7))});
    return scene;
}

}  // namespace

TEST_CASE("golden: Scene → raster 终端外观（盲文描边 + 半块填充）") {
    canvas::Canvas canvas(26, 11);
    canvas::render_scene(fixed_scene(), canvas);

    backend::snapshot::AnsiBackend backend;
    golden::check_or_update("scene_raster", "basic",
                            backend::snapshot::render_snapshot(backend, canvas.to_buffer()));
}
