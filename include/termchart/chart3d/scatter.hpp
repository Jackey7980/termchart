// scatter.hpp —— 3D 散点图：把三维点云投影成 Scene（header-only）
//
// 不做终端专属渲染：产出的是与 2D 图表同构的 scene::Scene，交给已有的
// render_scene → Canvas → Renderer 管线，复用 braille/halfblock 光栅化。深度只用于
// 着色（近亮远暗），立体感由此而来，无需自造 z-buffer。
#ifndef TERMCHART_CHART3D_SCATTER_HPP
#define TERMCHART_CHART3D_SCATTER_HPP

#include <algorithm>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/math3d/camera.hpp"
#include "termchart/math3d/vec3.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"
#include "termchart/util/math.hpp"

namespace termchart::chart3d {

struct Scatter3dOpts {
    double cols = 72.0;        // 画布宽（格）
    double rows = 28.0;        // 画布高（格）
    double azimuth_deg = 35.0;   // 轨道方位角
    double elevation_deg = 22.0; // 轨道仰角
    double fovy_deg = 55.0;       // 垂直视场角
    double point_size = 0.9;      // 每个点在画布上的边长（格）
    bool axes = true;             // 是否画 X/Y/Z 轴
};

// 深度 t（0=近，1=远）→ 颜色：近处暖亮、远处冷暗。
inline Color shade(double t) noexcept {
    const double k = util::clamp<double>(t, 0.0, 1.0);
    const auto lerp_u8 = [](int a, int b, double kk) {
        return static_cast<std::uint8_t>(a + (b - a) * kk);
    };
    return Color::rgb(lerp_u8(255, 40, k), lerp_u8(210, 70, k), lerp_u8(80, 150, k));
}

// 自动取景：按点云包围盒居中、按半径定距离与近/远裁剪面。
inline math3d::Camera make_camera(double cols, double rows, double azimuth_deg,
                                  double elevation_deg, double fovy_deg,
                                  const math3d::Vec3& center, double radius) noexcept {
    math3d::Camera cam;
    cam.aspect = cols / rows;
    cam.fovy_deg = fovy_deg;
    const double fovy = fovy_deg * math3d::kDeg2Rad;
    const double dist = radius / std::tan(fovy / 2.0) * 1.8;
    cam.near = std::max(0.01, dist - radius * 1.5);
    cam.far = dist + radius * 1.5;
    cam.target = center;
    cam.orbit(azimuth_deg, elevation_deg, dist);
    return cam;
}

inline void bounding(const std::vector<math3d::Vec3>& pts, math3d::Vec3& minv,
                     math3d::Vec3& maxv, math3d::Vec3& center, double& radius) noexcept {
    if (pts.empty()) {
        return;
    }
    minv = maxv = pts.front();
    for (const auto& p : pts) {
        minv.x = std::min(minv.x, p.x);
        minv.y = std::min(minv.y, p.y);
        minv.z = std::min(minv.z, p.z);
        maxv.x = std::max(maxv.x, p.x);
        maxv.y = std::max(maxv.y, p.y);
        maxv.z = std::max(maxv.z, p.z);
    }
    center = (minv + maxv) * 0.5;
    radius = (maxv - minv).length() * 0.5;
    if (!(radius > 0.0)) {
        radius = 1.0;
    }
}

inline scene::Scene scatter3d(const std::vector<math3d::Vec3>& pts, const Scatter3dOpts& o = {}) {
    scene::Scene s;
    if (pts.empty()) {
        return s;
    }

    math3d::Vec3 minv, maxv, center;
    double radius = 0.0;
    bounding(pts, minv, maxv, center, radius);

    const math3d::Camera cam =
        make_camera(o.cols, o.rows, o.azimuth_deg, o.elevation_deg, o.fovy_deg, center, radius);

    // 坐标轴：从包围盒最小角沿三轴方向各画一条，颜色固定 X红/Y绿/Z蓝。
    if (o.axes) {
        const math3d::Vec3 axes[3] = {math3d::Vec3{maxv.x, minv.y, minv.z},
                                      math3d::Vec3{minv.x, maxv.y, minv.z},
                                      math3d::Vec3{minv.x, minv.y, maxv.z}};
        const char* labels[3] = {"X", "Y", "Z"};
        const Color axis_color[3] = {Color::rgb(230, 80, 80), Color::rgb(90, 200, 110),
                                     Color::rgb(90, 140, 230)};
        for (int a = 0; a < 3; ++a) {
            const auto p0 = cam.project(minv, o.cols, o.rows);
            const auto p1 = cam.project(axes[a], o.cols, o.rows);
            if (!p0.visible || !p1.visible) {
                continue;
            }
            s.push(scene::StrokeOp{axis_color[a], 1.0});
            s.push(scene::MoveTo{Point{p0.x, p0.y}});
            s.push(scene::LineTo{Point{p1.x, p1.y}});
            s.push(scene::TextOp{Point{p1.x, p1.y}, std::string(labels[a]), Style{}});
        }
    }

    // 点：每个点一个填充小方块，颜色随深度变化。
    const double h = o.point_size / 2.0;
    for (const auto& p : pts) {
        const auto pr = cam.project(p, o.cols, o.rows);
        if (!pr.visible) {
            continue;
        }
        s.push(scene::FillOp{shade(pr.depth)});
        s.push(scene::RectOp{Rect{pr.x - h, pr.y - h, o.point_size, o.point_size}});
    }
    return s;
}

}  // namespace termchart::chart3d

#endif  // TERMCHART_CHART3D_SCATTER_HPP
