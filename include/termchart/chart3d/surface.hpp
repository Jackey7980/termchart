// surface.hpp —— 3D 曲面/网格图：把 z=f(x,y) 投影成 Scene 线框（header-only）
//
// 与 scatter 同构：只产出 scene::Scene，复用现有终端光栅管线。曲面以**线框**呈现
// （每行/每列相邻的网格点连线），连线颜色按平均深度着色；这也是 3D 曲面最易辨识、
// 最稳的画法，避免在终端上赌多边形填充的清晰度。
#ifndef TERMCHART_CHART3D_SURFACE_HPP
#define TERMCHART_CHART3D_SURFACE_HPP

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/chart3d/common.hpp"
#include "termchart/chart3d/projector.hpp"
#include "termchart/math3d/camera.hpp"
#include "termchart/math3d/vec3.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"
#include "termchart/util/math.hpp"

namespace termchart::chart3d {

struct Surface3dOpts {
    double cols = 72.0;
    double rows = 28.0;
    double azimuth_deg = 35.0;   // projector=nullptr 时生效
    double elevation_deg = 25.0;  // projector=nullptr 时生效
    double fovy_deg = 55.0;       // projector=nullptr 时生效
    int res_x = 24;  // 网格列数
    int res_y = 24;  // 网格行数
    bool axes = true;
};

// projector 为 nullptr 时按 opts 现算 Projector3D（既有行为，逐字节不变）；
// 非空则全权委托注入的投影器（opts 里的视角参数被忽略），为 Projector4D
// 等留出注入缝。
inline scene::Scene surface3d(const std::function<double(double, double)>& f, double x0, double x1,
                              double y0, double y1, const Surface3dOpts& o = {},
                              const IProjector* projector = nullptr) {
    scene::Scene s;
    const std::size_t nx = static_cast<std::size_t>(std::max(1, o.res_x));
    const std::size_t ny = static_cast<std::size_t>(std::max(1, o.res_y));

    // 采样网格，记录包围盒以自动取景。
    std::vector<std::vector<math3d::Vec3>> grid(ny + 1, std::vector<math3d::Vec3>(nx + 1));
    math3d::Vec3 minv{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
                      std::numeric_limits<double>::infinity()};
    math3d::Vec3 maxv{-std::numeric_limits<double>::infinity(),
                      -std::numeric_limits<double>::infinity(),
                      -std::numeric_limits<double>::infinity()};
    for (std::size_t j = 0; j <= ny; ++j) {
        const double yy = util::lerp(y0, y1, static_cast<double>(j) / static_cast<double>(ny));
        for (std::size_t i = 0; i <= nx; ++i) {
            const double xx = util::lerp(x0, x1, static_cast<double>(i) / static_cast<double>(nx));
            const double zz = f(xx, yy);
            const math3d::Vec3 p{xx, yy, zz};
            grid[j][i] = p;
            minv.x = std::min(minv.x, p.x);
            minv.y = std::min(minv.y, p.y);
            minv.z = std::min(minv.z, p.z);
            maxv.x = std::max(maxv.x, p.x);
            maxv.y = std::max(maxv.y, p.y);
            maxv.z = std::max(maxv.z, p.z);
        }
    }

    const math3d::Vec3 center = (minv + maxv) * 0.5;
    double radius = (maxv - minv).length() * 0.5;
    if (!(radius > 0.0)) {
        radius = 1.0;
    }

    const Projector3D default_projector{
        make_camera(o.cols, o.rows, o.azimuth_deg, o.elevation_deg, o.fovy_deg, center, radius)};
    const IProjector& proj = (projector != nullptr) ? *projector : default_projector;

    if (o.axes) {
        const math3d::Vec3 axes[3] = {math3d::Vec3{maxv.x, minv.y, minv.z},
                                      math3d::Vec3{minv.x, maxv.y, minv.z},
                                      math3d::Vec3{minv.x, minv.y, maxv.z}};
        const char* labels[3] = {"X", "Y", "Z"};
        const Color axis_color[3] = {Color::rgb(230, 80, 80), Color::rgb(90, 200, 110),
                                     Color::rgb(90, 140, 230)};
        for (int a = 0; a < 3; ++a) {
            const auto p0 = proj.project(minv, o.cols, o.rows);
            const auto p1 = proj.project(axes[a], o.cols, o.rows);
            if (!p0.visible || !p1.visible) {
                continue;
            }
            s.push(scene::StrokeOp{axis_color[a], 1.0});
            s.push(scene::MoveTo{Point{p0.x, p0.y}});
            s.push(scene::LineTo{Point{p1.x, p1.y}});
            s.push(scene::TextOp{Point{p1.x, p1.y}, std::string(labels[a]), Style{}});
        }
    }

    // 线框：逐行、逐列相邻点连线，颜色按平均深度着色。
    const auto seg = [&](const math3d::Vec3& a, const math3d::Vec3& b) {
        const auto pa = proj.project(a, o.cols, o.rows);
        const auto pb = proj.project(b, o.cols, o.rows);
        if (!pa.visible || !pb.visible) {
            return;
        }
        s.push(scene::StrokeOp{shade((pa.depth + pb.depth) * 0.5), 1.0});
        s.push(scene::MoveTo{Point{pa.x, pa.y}});
        s.push(scene::LineTo{Point{pb.x, pb.y}});
    };
    for (std::size_t j = 0; j <= ny; ++j) {
        for (std::size_t i = 0; i < nx; ++i) {
            seg(grid[j][i], grid[j][i + 1]);
        }
    }
    for (std::size_t i = 0; i <= nx; ++i) {
        for (std::size_t j = 0; j < ny; ++j) {
            seg(grid[j][i], grid[j + 1][i]);
        }
    }
    return s;
}

}  // namespace termchart::chart3d

#endif  // TERMCHART_CHART3D_SURFACE_HPP
