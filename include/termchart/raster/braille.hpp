// braille.hpp —— 盲文 2×4 子像素光栅器（分辨率最高）
//
// 每个终端格 = 2 列 × 4 行的点阵，点亮的点拼成 U+2800 系盲文字形。子像素→点
// 位的几何约定全部在 detail/braille_table.hpp，本文件只做"采样 + 查表"。
#ifndef TERMCHART_RASTER_BRAILLE_HPP
#define TERMCHART_RASTER_BRAILLE_HPP

#include "termchart/raster/rasterizer.hpp"

namespace termchart::raster {

class BrailleRasterizer final : public Rasterizer {
public:
    [[nodiscard]] CellBuffer rasterize(const image::FloatField& field,
                                       const RasterOptions& opts) const override;
    [[nodiscard]] std::uint32_t subpixel_width() const noexcept override { return 2; }
    [[nodiscard]] std::uint32_t subpixel_height() const noexcept override { return 4; }
    [[nodiscard]] RasterKind kind() const noexcept override { return RasterKind::Braille; }
    [[nodiscard]] const char* name() const noexcept override { return "braille"; }
};

}  // namespace termchart::raster

#endif  // TERMCHART_RASTER_BRAILLE_HPP
