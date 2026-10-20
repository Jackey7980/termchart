// wire.hpp —— 4D 线框/点云 → scene::Scene（header-only，chart3d 的 4D 对应物）
//
// 与 chart3d/scatter.hpp 同构：发射器只管几何与图元，视图全权委托 Projector4D
// （两阶段 4D→2D）。每条边的两端经投影器落到画布，两端均可见才画；顶点可选画
// 成按深度着色的小方块。边按两端平均深度着色，给旋转中的 4D 形体以深度提示。
#ifndef TERMCHART_CHART4D_WIRE_HPP
#define TERMCHART_CHART4D_WIRE_HPP

#include <cstdint>
#include <utility>
#include <vector>

#include "termchart/chart3d/common.hpp"    // chart3d::shade（深度→颜色）
#include "termchart/chart4d/projector4d.hpp"
#include "termchart/core/point.hpp"  // Point
#include "termchart/core/rect.hpp"   // Rect
#include "termchart/core/style.hpp"  // Color
#include "termchart/math4d/vec4.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"

namespace termchart::chart4d {

struct Wire4dOpts {
    double cols = 72.0;       // 画布宽（格）
    double rows = 28.0;       // 画布高（格）
    double point_size = 0.9;  // 顶点方块边长（格）
    bool draw_vertices = true;  // 是否在边之外叠加顶点标记
};

// 一条边 = 顶点索引对（超立方体等线框模型的边表即为此型的 vector）。
using Edge = std::pair<std::uint32_t, std::uint32_t>;

// 把 4D 顶点集 + 边表发射成 Scene。越界索引被跳过（不抛异常）；两端投影后
// 任一不可见（4D 相机后方/近裁剪，或 3D 相机视锥外）的边被整段丢弃。
inline scene::Scene wire4d(const std::vector<math4d::Vec4>& verts,
                           const std::vector<Edge>& edges, const Wire4dOpts& o,
                           const Projector4D& projector) {
    scene::Scene s;
    const double h = o.point_size / 2.0;

    for (const auto& e : edges) {
        if (e.first >= verts.size() || e.second >= verts.size()) {
            continue;
        }
        const auto p0 = projector.project(verts[e.first], o.cols, o.rows);
        const auto p1 = projector.project(verts[e.second], o.cols, o.rows);
        if (!p0.visible || !p1.visible) {
            continue;
        }
        s.push(scene::StrokeOp{chart3d::shade((p0.depth + p1.depth) * 0.5), 1.0});
        s.push(scene::MoveTo{Point{p0.x, p0.y}});
        s.push(scene::LineTo{Point{p1.x, p1.y}});
    }

    if (o.draw_vertices) {
        for (const auto& v : verts) {
            const auto p = projector.project(v, o.cols, o.rows);
            if (!p.visible) {
                continue;
            }
            s.push(scene::FillOp{chart3d::shade(p.depth)});
            s.push(scene::RectOp{Rect{p.x - h, p.y - h, o.point_size, o.point_size}});
        }
    }
    return s;
}

}  // namespace termchart::chart4d

#endif  // TERMCHART_CHART4D_WIRE_HPP
