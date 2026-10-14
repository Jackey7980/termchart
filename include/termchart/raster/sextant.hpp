// sextant.hpp —— 六分仪字符光栅器（2×3 子像素，Unicode 1FB00 区）
//
// 保真度介于象限(2×2=4) 与盲文(2×4=8) 之间，是终端字符图里"中等保真度"的一档。
// 每个字符格切成 2 列 × 3 行的 6 个 sextant 区域，按填充情况映射到 Unicode
// "Symbols for Legacy Computing" 的六分仪块（U+1FB00 起，码点 = 0x1FB00 + 6 位掩码；
// 位序约定：左上 0 / 左中 1 / 左下 2 / 右上 3 / 右中 4 / 右下 5）。
#ifndef TERMCHART_RASTER_SEXTANT_HPP
#define TERMCHART_RASTER_SEXTANT_HPP

#include "termchart/export.hpp"
#include "termchart/raster/rasterizer.hpp"

namespace termchart::raster {

class SextantRasterizer final : public Rasterizer {
public:
    [[nodiscard]] std::uint32_t subpixel_width() const noexcept override { return 2; }
    [[nodiscard]] std::uint32_t subpixel_height() const noexcept override { return 3; }
    [[nodiscard]] RasterKind kind() const noexcept override { return RasterKind::Sextant; }
    [[nodiscard]] const char* name() const noexcept override { return "sextant"; }
    CellBuffer rasterize(const image::FloatField& field,
                         const RasterOptions& opts) const override;
};

}  // namespace termchart::raster

#endif  // TERMCHART_RASTER_SEXTANT_HPP
