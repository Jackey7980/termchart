// projector4d.cpp —— Projector4D 的两阶段实现 + vtable 锚点
//
// vtable/typeinfo 落本 TU：projector4d.hpp 的两个 project 都在此定义（非内联），
// 作为 key function 让 Projector4D 的虚表只在这一个 TU 发强符号。
#include "termchart/chart4d/projector4d.hpp"

namespace termchart::chart4d {

Projector4D::Projector4D(math4d::Mat5 rotation, math4d::Camera4 camera,
                         std::shared_ptr<chart3d::IProjector> inner) noexcept
    : rotation_(rotation), camera_(camera), inner_(std::move(inner)) {}

math3d::Projected Projector4D::project(const math4d::Vec4& world, double vw,
                                       double vh) const noexcept {
    if (inner_ == nullptr) {
        return {};  // 没有第二阶段可委托：无从得到画布坐标
    }
    // 阶段一：4D 旋转 + 透视除以 W。
    const math4d::Vec4 rotated = rotation_.transform(world);
    const math4d::Projected4 p4 = camera_.project(rotated);
    if (!p4.visible) {
        return {};  // 相机后方/近裁剪：整条边/点不可见
    }
    // 阶段二：Vec3 交内层 3D 投影器出画布。
    return inner_->project(p4.xyz, vw, vh);
}

math3d::Projected Projector4D::project(const math3d::Vec3& world, double vw,
                                       double vh) const noexcept {
    if (inner_ == nullptr) {
        return {};
    }
    return inner_->project(world, vw, vh);  // 3D 点直通内层，4D 阶段不参与
}

}  // namespace termchart::chart4d
