// 34_export_svg.cpp —— 矢量导出：Scene → SVG（写 stdout）
#include <cmath>

#include "termchart/api/print.hpp"
#include "termchart/backend/vector/svg_backend.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    scene::Scene scene;
    const double w = 60.0;
    const double h = 20.0;
    scene.push(scene::RectOp{Rect{0.0, 0.0, w, h}});
    scene.push(scene::StrokeOp{Color::ansi16(8), 1.0});

    // 正弦折线
    scene.push(scene::MoveTo{Point{0.0, h / 2.0}});
    for (int i = 0; i <= 120; ++i) {
        const double x = w * static_cast<double>(i) / 120.0;
        const double y = h / 2.0 - (h / 2.0 - 1.0) * std::sin(x / 6.0);
        scene.push(scene::LineTo{Point{x, y}});
    }
    scene.push(scene::StrokeOp{Color::ansi16(2), 1.5});
    scene.push(scene::TextOp{Point{1.0, 1.0}, "sin(x)", Style{}.set_fg(Color::ansi16(6))});

    backend::vector::SvgBackend backend;
    backend.set_options(backend::vector::VectorOptions{.scale = 10.0});
    const std::string svg = backend::vector::render_vector(backend, scene);

    const Status st = api::write_stdout(svg);
    api::flush_stdout();
    return st == ErrorCode::Ok ? 0 : 1;
}
