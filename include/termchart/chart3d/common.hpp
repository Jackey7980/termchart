// common.hpp —— chart3d 模块共享的 3D 着色/取景辅助（header-only）
//
// scatter3d 与 surface3d 都用得到：深度→颜色着色（shade）与按包围盒自动取景
// （make_camera）。抽到独立头，保证 scatter.hpp / surface.hpp 都能独立 include。
#ifndef TERMCHART_CHART3D_COMMON_HPP
#define TERMCHART_CHART3D_COMMON_HPP

#include <cstdint>

#include "termchart/core/style.hpp"    // Color
#include "termchart/math3d/camera.hpp"  // Camera, kDeg2Rad
#include "termchart/math3d/vec3.hpp"    // Vec3
#include "termchart/util/math.hpp"      // clamp, max

namespace termchart::chart3d {

// 深度 t（0=近，1=远）→ 颜色：近处暖亮、远处冷暗。
inline Color shade(double t) noexcept {
    const double k = util::clamp<double>(t, 0.0, 1.0);
    const auto lerp_u8 = [](int a, int b, double kk) {
        return static_cast<std::uint8_t>(a + (b - a) * kk);
    };
    return Color::rgb(lerp_u8(255, 40, k), lerp_u8(210, 70, k), lerp_u8(80, 150, k));
}

// 自动取景：按点云/曲面包围盒居中、按半径定距离与近/远裁剪面。
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

}  // namespace termchart::chart3d

#endif  // TERMCHART_CHART3D_COMMON_HPP
