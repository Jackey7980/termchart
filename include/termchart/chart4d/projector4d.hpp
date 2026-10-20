// projector4d.hpp —— 4D 投影器：实现 chart3d::IProjector，串起两阶段 4D→2D
//
// 数据流（用户口径："Vec4 -> Projector4D -> Vec3 -> Projector3D -> Vec2"）：
//   阶段一（math4d，本类持有）：Vec4 经 4D 旋转（6 平面组合）→ 透视除以 W → Vec3；
//   阶段二（内层 IProjector，通常是 chart3d::Projector3D）：Vec3 → 画布 Projected。
// 两个阶段各自可替换：换旋转/换 4D 相机只动阶段一；换 3D 视图（正交、弱透视、
// 自定义轨道相机）只换内层 IProjector——这正是当初把 chart3d 抽成 IProjector 的目的。
//
// 为什么继承 IProjector 却又多一个 Vec4 入口：IProjector::project 的入参是
// Vec3（3D 缝），4D 点无法穿过它。故本类：
//   * 新增 project(const Vec4&, ...) —— 真正的两阶段 4D 入口（线框/超立方体走这条）；
//   * 覆写 project(const Vec3&, ...) —— 满足 IProjector：喂 3D 点时等价于内层
//     投影器（4D 旋转/透视不参与），使本类也能直接插进 scatter3d/surface3d。
#ifndef TERMCHART_CHART4D_PROJECTOR4D_HPP
#define TERMCHART_CHART4D_PROJECTOR4D_HPP

#include <memory>

#include "termchart/chart3d/projector.hpp"  // chart3d::IProjector, math3d::Projected
#include "termchart/export.hpp"
#include "termchart/math4d/camera4.hpp"  // math4d::Camera4, Projected4
#include "termchart/math4d/mat5.hpp"     // math4d::Mat5
#include "termchart/math4d/vec4.hpp"     // math4d::Vec4

namespace termchart::chart4d {

class TERMCHART_API Projector4D final : public chart3d::IProjector {
public:
    Projector4D(math4d::Mat5 rotation, math4d::Camera4 camera,
                std::shared_ptr<chart3d::IProjector> inner) noexcept;

    // 4D 入口——真正的两阶段流程：
    //   Vec4 --(4D 旋转 rotation_)--> Vec4 --(透视除 W camera_)--> Vec3
    //        --(内层 inner_->project)--> 画布 Projected。
    // 内层为空或阶段一判定不可见（相机后方/近裁剪）→ 返回 visible=false。
    [[nodiscard]] math3d::Projected project(const math4d::Vec4& world, double vw,
                                            double vh) const noexcept;

    // 实现 IProjector：直接喂 3D 点时等价于内层投影器（4D 阶段不参与）。
    [[nodiscard]] math3d::Projected project(const math3d::Vec3& world, double vw,
                                            double vh) const noexcept override;

private:
    math4d::Mat5 rotation_;
    math4d::Camera4 camera_;
    std::shared_ptr<chart3d::IProjector> inner_;
};

}  // namespace termchart::chart4d

#endif  // TERMCHART_CHART4D_PROJECTOR4D_HPP
