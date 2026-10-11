// ascii.hpp —— 密度字符 1×1 光栅器（最朴素、零依赖）
//
// 用 . : - = + * # % @ 把强度映射成由淡到浓的 ASCII 字符。适合纯文本环境、
// 不能显示块字符的终端，以及 golden 快照的可读对照。
#ifndef TERMCHART_RASTER_ASCII_HPP
#define TERMCHART_RASTER_ASCII_HPP

#include "termchart/raster/rasterizer.hpp"

namespace termchart::raster {

class AsciiRasterizer final : public Rasterizer {
public:
    [[nodiscard]] CellBuffer rasterize(const image::FloatField& field,
                                       const RasterOptions& opts) const override;
    [[nodiscard]] std::uint32_t subpixel_width() const noexcept override { return 1; }
    [[nodiscard]] std::uint32_t subpixel_height() const noexcept override { return 1; }
    [[nodiscard]] RasterKind kind() const noexcept override { return RasterKind::Ascii; }
    [[nodiscard]] const char* name() const noexcept override { return "ascii"; }
};

}  // namespace termchart::raster

#endif  // TERMCHART_RASTER_ASCII_HPP
