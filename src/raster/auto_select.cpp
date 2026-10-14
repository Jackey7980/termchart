#include "termchart/raster/auto_select.hpp"

namespace termchart::raster {

namespace {

bool supported(const RasterCaps& caps, RasterKind kind) noexcept {
    switch (kind) {
        case RasterKind::Braille:
            return caps.braille;
        case RasterKind::HalfBlock:
            return caps.halfblock;
        case RasterKind::Quadrant:
            return caps.quadrant;
        case RasterKind::Block:
            return caps.block;
        case RasterKind::Ascii:
            return caps.ascii;
        case RasterKind::Sextant:
            return caps.sextant;
    }
    return false;
}

// 保真度降序：分辨率从高到低。
constexpr RasterKind kFallbackOrder[] = {
    RasterKind::Braille,
    RasterKind::Sextant,
    RasterKind::Quadrant,
    RasterKind::HalfBlock,
    RasterKind::Block,
    RasterKind::Ascii,
};

}  // namespace

RasterKind pick_raster(const RasterCaps& caps, RasterKind preferred) noexcept {
    if (supported(caps, preferred)) {
        return preferred;
    }
    for (RasterKind kind : kFallbackOrder) {
        if (supported(caps, kind)) {
            return kind;
        }
    }
    return RasterKind::Ascii;  // 最后兜底：任何终端都认密度字符
}

}  // namespace termchart::raster
