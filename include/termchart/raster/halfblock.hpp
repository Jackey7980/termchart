// halfblock.hpp —— 半块 1×2 光栅器（上下双色）
//
// 一个格 = 上、下两半。▀ 上半亮、▄ 下半亮、█ 全亮、空格全灭。因为半块字符让
// 上/下两半分别取前景/背景，单格即可表现两种颜色——这是比盲文更"彩色"的选择。
#ifndef TERMCHART_RASTER_HALFBLOCK_HPP
#define TERMCHART_RASTER_HALFBLOCK_HPP

#include "termchart/raster/rasterizer.hpp"

namespace termchart::raster {

class HalfBlockRasterizer final : public Rasterizer {
public:
    [[nodiscard]] CellBuffer rasterize(const image::FloatField& field,
                                       const RasterOptions& opts) const override;
    [[nodiscard]] std::uint32_t subpixel_width() const noexcept override { return 1; }
    [[nodiscard]] std::uint32_t subpixel_height() const noexcept override { return 2; }
    [[nodiscard]] RasterKind kind() const noexcept override { return RasterKind::HalfBlock; }
    [[nodiscard]] const char* name() const noexcept override { return "halfblock"; }
};

}  // namespace termchart::raster

#endif  // TERMCHART_RASTER_HALFBLOCK_HPP
