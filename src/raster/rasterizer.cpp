#include "termchart/raster/rasterizer.hpp"

#include "termchart/raster/ascii.hpp"
#include "termchart/raster/block.hpp"
#include "termchart/raster/braille.hpp"
#include "termchart/raster/halfblock.hpp"
#include "termchart/raster/quadrant.hpp"

namespace termchart::raster {

Rasterizer::~Rasterizer() = default;

CellBuffer rasterize(const image::FloatField& field, RasterKind kind,
                     const RasterOptions& opts) {
    switch (kind) {
        case RasterKind::Braille:
            return BrailleRasterizer{}.rasterize(field, opts);
        case RasterKind::HalfBlock:
            return HalfBlockRasterizer{}.rasterize(field, opts);
        case RasterKind::Quadrant:
            return QuadrantRasterizer{}.rasterize(field, opts);
        case RasterKind::Ascii:
            return AsciiRasterizer{}.rasterize(field, opts);
        case RasterKind::Block:
            return BlockRasterizer{}.rasterize(field, opts);
    }
    return CellBuffer{};
}

const char* raster_name(RasterKind kind) noexcept {
    switch (kind) {
        case RasterKind::Braille:
            return "braille";
        case RasterKind::HalfBlock:
            return "halfblock";
        case RasterKind::Quadrant:
            return "quadrant";
        case RasterKind::Ascii:
            return "ascii";
        case RasterKind::Block:
            return "block";
    }
    return "unknown";
}

}  // namespace termchart::raster
