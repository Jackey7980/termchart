// projector.hpp —— chart3d 的投影缝：3D 世界坐标 → 画布坐标的多态接口
//
// 为什么抽接口：scatter3d / surface3d 原先直接持有 math3d::Camera（Mat4
// 视图×投影），把"怎么看"焊死在"画什么"里。抽 IProjector 之后：
//   * Projector3D：现有 Camera 的薄封装（默认实现，行为逐字节不变）；
//   * 未来 Projector4D：4D→3D 投影产出 Vec3 序列，再经本接口完成
//     3D→2D——两阶段各自可替换，scatter/surface 与 chart4d 都不用改；
//   * 其他投影算法（正交 / 弱透视 / 自定义相机）同样只实现本接口即可注入。
//
// 注入方式：scatter3d / surface3d 的可选尾参数 `projector`，nullptr =
// 按 opts 的 azimuth/elevation/fovy 现算 Projector3D（既有调用方零改动，
// opts 里的视角参数在注入投影器时被忽略——视图归投影器所有）。
#ifndef TERMCHART_CHART3D_PROJECTOR_HPP
#define TERMCHART_CHART3D_PROJECTOR_HPP

#include "termchart/export.hpp"
#include "termchart/math3d/camera.hpp"  // math3d::Projected
#include "termchart/math3d/vec3.hpp"    // math3d::Vec3

namespace termchart::chart3d {

class TERMCHART_API IProjector {
public:
    // 类内只声明、src/chart3d/projector.cpp 内定义（key function 惯用法）：
    // vtable/typeinfo 只在这一个 TU 发强符号。
    virtual ~IProjector();

    // 世界坐标 → 画布坐标。vw/vh 为画布尺寸（格，左上原点、y 向下）。
    // 返回值语义与 math3d::Camera::project 一致：depth 归一化（0=近、
    // 1=远，供着色），visible 表示在视锥内且位于相机前方。
    [[nodiscard]] virtual math3d::Projected project(const math3d::Vec3& world, double vw,
                                                    double vh) const noexcept = 0;
};

// 默认实现：把 math3d::Camera（轨道相机 + 透视投影 + 视锥裁剪）包一层。
// 按值持有 Camera（拷贝便宜：三个 Vec3 + 5 个 double），投影原样转发。
class TERMCHART_API Projector3D final : public IProjector {
public:
    explicit Projector3D(math3d::Camera camera) noexcept;

    [[nodiscard]] math3d::Projected project(const math3d::Vec3& world, double vw,
                                            double vh) const noexcept override;

private:
    math3d::Camera camera_;
};

}  // namespace termchart::chart3d

#endif  // TERMCHART_CHART3D_PROJECTOR_HPP
