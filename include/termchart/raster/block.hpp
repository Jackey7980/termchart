// block.hpp —— 八级竖条 1×1 光栅器
//
// 每格按平均强度取 1/8 高度的填充块（▁▂▃▄▅▆▇█）。分辨率最低但零子像素开销，
// 适合大色块、进度条、密度概览。invert 翻转强度（照片负片）。
#ifndef TERMCHART_RASTER_BLOCK_HPP
#define TERMCHART_RASTER_BLOCK_HPP

#include "termchart/raster/rasterizer.hpp"

namespace termchart::raster {

class BlockRasterizer final : public Rasterizer {
public:
    [[nodiscard]] CellBuffer rasterize(const image::FloatField& field,
                                       const RasterOptions& opts) const override;
    [[nodiscard]] std::uint32_t subpixel_width() const noexcept override { return 1; }
    [[nodiscard]] std::uint32_t subpixel_height() const noexcept override { return 1; }
    [[nodiscard]] RasterKind kind() const noexcept override { return RasterKind::Block; }
    [[nodiscard]] const char* name() const noexcept override { return "block"; }
};

}  // namespace termchart::raster

#endif  // TERMCHART_RASTER_BLOCK_HPP
