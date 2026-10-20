// camera4.hpp —— 4D→3D 透视投影（header-only，"除以 W"的齐次除法）
//
// 这是 4D 管线第一阶段的世界→3D 相机：把 4D 相机架在第 4 轴（W）上、看向
// 原点，对一个 Vec4 做透视除法，产出 math3d::Vec3（交给下游 chart3d 的 3D
// 轨道相机做第二阶段 3D→2D）。与 math3d::Camera 的关系：后者吃 Vec3 出画布
// 坐标，本结构吃 Vec4 出 Vec3——两者串起来就是完整的 4D→2D 链。
//
// 之所以返回 math3d::Vec3 而不是 math4d 自有类型：3D 向量的语义归 math3d，
// 且下游（chart4d）本来就要把它喂给 math3d 的相机；math4d 与 math3d 同为 L3，
// 该依赖方向被层号允许。
#ifndef TERMCHART_MATH4D_CAMERA4_HPP
#define TERMCHART_MATH4D_CAMERA4_HPP

#include <cmath>

#include "termchart/math3d/vec3.hpp"  // math3d::Vec3（4D→3D 的输出类型）
#include "termchart/math4d/vec4.hpp"

namespace termchart::math4d {

constexpr double kDeg2Rad = 0.017453292519943295769;  // π/180（与 math3d::kDeg2Rad 同值）

// 4D→3D 投影结果。xyz 即投影到的 3D 点；w_view 是相机空间深度
// （distance - w，越大越远），供着色/调试；visible 表示位于相机前方且
// 不在近裁剪面之内。
struct Projected4 {
    math3d::Vec3 xyz{};
    double w_view = 0.0;
    bool visible = false;
};

struct Camera4 {
    double distance = 3.0;  // 4D 相机置于 +W 轴、距原点 distance，看向 -W（朝原点）
    double focal = 2.0;     // W 向焦距：投影缩放 = focal / w_view（越大视野越窄）
    double near = 0.1;      // 近裁剪：w_view 的下界（含）以外的点不可见

    // 透视除以 W：先把点移到相机前方深度 w_view = distance - p.w，再按
    // focal / w_view 缩放 xyz。w_view <= near（相机处、后方或太近）→ 不可见，
    // 避免除出巨大/负值坐标（与 math3d::Camera 的 w<=0 早退同思路）。
    Projected4 project(const Vec4& p) const noexcept {
        Projected4 r;
        const double w_view = distance - p.w;
        if (!(w_view > near)) {
            return r;  // NaN 也走这里：!(NaN>near) 为真
        }
        const double k = focal / w_view;
        r.xyz = math3d::Vec3{p.x * k, p.y * k, p.z * k};
        r.w_view = w_view;
        r.visible = true;
        return r;
    }
};

// 由 W 向垂直视场角推焦距：f = 1 / tan(fov/2)，与 math3d 透视的 f 同式。
// fov_deg 越视野小、焦距越大（放大）；60° → ≈1.732。
inline double focal_from_fov_deg(double fov_deg) noexcept {
    return 1.0 / std::tan(fov_deg * kDeg2Rad / 2.0);
}

}  // namespace termchart::math4d

#endif  // TERMCHART_MATH4D_CAMERA4_HPP
