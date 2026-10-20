// projector.cpp —— IProjector 的 vtable 锚点 + Projector3D 的 Camera 转发
#include "termchart/chart3d/projector.hpp"

namespace termchart::chart3d {

IProjector::~IProjector() = default;

Projector3D::Projector3D(math3d::Camera camera) noexcept : camera_(camera) {}

math3d::Projected Projector3D::project(const math3d::Vec3& world, double vw,
                                       double vh) const noexcept {
    return camera_.project(world, vw, vh);
}

}  // namespace termchart::chart3d
