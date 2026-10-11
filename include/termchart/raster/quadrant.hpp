// quadrant.hpp —— 象限块 2×2 光栅器
//
// 一个格 = 2×2 子像素。标准象限块字符（U+2596–U+259F）只覆盖 10 种图案，
// 缺 3 点亮的组合，所以那种情况改画"唯一的熄灭象限"为熄灭色、其余为点亮色——
// 视觉效果等同 3 点亮。几何与点位约定见 src/raster/quadrant.cpp 的映射表。
#ifndef TERMCHART_RASTER_QUADRANT_HPP
#define TERMCHART_RASTER_QUADRANT_HPP

#include "termchart/raster/rasterizer.hpp"

namespace termchart::raster {

class QuadrantRasterizer final : public Rasterizer {
public:
    [[nodiscard]] CellBuffer rasterize(const image::FloatField& field,
                                       const RasterOptions& opts) const override;
    [[nodiscard]] std::uint32_t subpixel_width() const noexcept override { return 2; }
    [[nodiscard]] std::uint32_t subpixel_height() const noexcept override { return 2; }
    [[nodiscard]] RasterKind kind() const noexcept override { return RasterKind::Quadrant; }
    [[nodiscard]] const char* name() const noexcept override { return "quadrant"; }
};

}  // namespace termchart::raster

#endif  // TERMCHART_RASTER_QUADRANT_HPP
