// 72_3d.cpp —— 3D 曲面/散点（规格外补充示例）
//
// 演示 chart3d：把 z=f(x,y) 投影成 scene::Scene，再走已有的 render_scene → Canvas →
// Renderer 终端管线（braille/halfblock）。深度只用于着色，不另造 z-buffer。改 f 与
// 视角（azimuth/elevation）即可换任意曲面。
#include <cmath>
#include <cstdint>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/scene_renderer.hpp"
#include "termchart/chart3d/scatter.hpp"
#include "termchart/chart3d/surface.hpp"
#include "termchart/math3d/vec3.hpp"
#include "termchart/render/renderer.hpp"

namespace {
using namespace termchart;
}  // namespace

int main() {
    const std::uint32_t cols = 90;
    const std::uint32_t rows = 34;
    canvas::Canvas canvas(cols, rows);

    chart3d::Surface3dOpts o;
    o.cols = static_cast<double>(cols);
    o.rows = static_cast<double>(rows);
    o.azimuth_deg = 35.0;
    o.elevation_deg = 28.0;
    o.res_x = 28;
    o.res_y = 22;

    // 马鞍面 z = sin(x)*cos(y)：四个对角隆起/下陷，立体感最直观。
    const auto surface = chart3d::surface3d(
        [](double x, double y) { return std::sin(x) * std::cos(y); }, -3.0, 3.0, -3.0, 3.0, o);
    canvas::render_scene(surface, canvas);

    // 在同张画布上叠几个采样散点，验证 scatter 与 surface 共用同一投影。
    std::vector<math3d::Vec3> pts;
    for (int i = 0; i < 12; ++i) {
        const double x = -3.0 + 0.5 * i;
        const double y = std::sin(x);
        pts.push_back(math3d::Vec3{x, y, std::sin(x) * std::cos(y)});
    }
    const auto cloud = chart3d::scatter3d(pts, chart3d::Scatter3dOpts{
                                                    static_cast<double>(cols),
                                                    static_cast<double>(rows), 35.0, 28.0, 55.0, 0.9,
                                                    false});
    canvas::render_scene(cloud, canvas);

    render::Renderer renderer;
    (void)api::write_stdout(renderer.render_full(canvas.to_buffer()));
    (void)api::write_stdout("\n");
    (void)api::flush_stdout();
    return 0;
}
