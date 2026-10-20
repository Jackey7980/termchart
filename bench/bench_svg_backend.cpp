// bench_svg_backend.cpp —— 矢量导出基线
//
// 度量 Scene → SVG（`SvgBackend` + `render_vector`）在不同几何密度下的耗时。复用既有
// `backend::vector::SvgBackend`，不另造导出入口。关注点：线段数翻倍时耗时增长是否接近
// 线性（path 序列化 O(图元)）。
#include <cstddef>
#include <cmath>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "bench.hpp"
#include "termchart/backend/vector/svg_backend.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"

namespace {

using namespace termchart;

scene::Scene make_scene(double w, double h, int segments) {
    scene::Scene scene;
    scene.push(scene::StrokeOp{Color::ansi16(8), 1.0});
    scene.push(scene::RectOp{Rect{0.0, 0.0, w, h}});
    scene.push(scene::StrokeOp{Color::ansi16(2), 1.5});
    scene.push(scene::MoveTo{Point{0.0, h / 2.0}});
    for (int i = 0; i <= segments; ++i) {
        const double x = w * static_cast<double>(i) / static_cast<double>(segments);
        const double y = h / 2.0 - (h / 2.0 - 1.0) * std::sin(x / 6.0);
        scene.push(scene::LineTo{Point{x, y}});
    }
    scene.push(scene::TextOp{Point{1.0, 1.0}, "sin(x)", Style{}.set_fg(Color::ansi16(6))});
    return scene;
}

}  // namespace

int main(int argc, char** argv) {
    const bool quick = bench::is_quick(argc, argv);
    const std::vector<std::tuple<double, double, int>> shapes =
        quick ? std::vector<std::tuple<double, double, int>>{{60.0, 20.0, 60}}
              : std::vector<std::tuple<double, double, int>>{
                    {60.0, 20.0, 60}, {120.0, 40.0, 240}, {240.0, 80.0, 480}};
    const int rounds = quick ? 3 : 20;

    bench::header("bench_svg_backend", "Scene → SVG（SvgBackend）");
    for (const auto& sh : shapes) {
        const auto w = std::get<0>(sh);
        const auto h = std::get<1>(sh);
        const int seg = std::get<2>(sh);
        const scene::Scene base = make_scene(w, h, seg);
        const std::string size = std::to_string(seg) + " segs";
        const double ns = bench::median_ns(
            [&base] {
                backend::vector::SvgBackend backend;
                backend.set_options(backend::vector::VectorOptions{.scale = 10.0});
                (void)backend::vector::render_vector(backend, base);
            },
            rounds);
        bench::report("svg", size, ns);
    }
    return 0;
}
