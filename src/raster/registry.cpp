// registry.cpp —— 光栅器注册表实现（内置 + 自定义登记）
#include "termchart/raster/registry.hpp"

#include <string>
#include <string_view>

#include "termchart/raster/ascii.hpp"
#include "termchart/raster/block.hpp"
#include "termchart/raster/braille.hpp"
#include "termchart/raster/halfblock.hpp"
#include "termchart/raster/quadrant.hpp"
#include "termchart/raster/sextant.hpp"

namespace termchart::raster {

RasterRegistry& RasterRegistry::instance() noexcept {
    static RasterRegistry registry;
    return registry;
}

void RasterRegistry::register_raster(std::string_view name, Creator creator) {
    custom_[std::string(name)] = std::move(creator);
}

bool RasterRegistry::supports_name(std::string_view name) const {
    if (custom_.find(name) != custom_.end()) {
        return true;
    }
    return name == "braille" || name == "halfblock" || name == "quadrant" ||
           name == "ascii" || name == "block" || name == "sextant";
}

std::unique_ptr<Rasterizer> RasterRegistry::create(RasterKind kind) const {
    switch (kind) {
        case RasterKind::Braille:
            return std::make_unique<BrailleRasterizer>();
        case RasterKind::HalfBlock:
            return std::make_unique<HalfBlockRasterizer>();
        case RasterKind::Quadrant:
            return std::make_unique<QuadrantRasterizer>();
        case RasterKind::Ascii:
            return std::make_unique<AsciiRasterizer>();
        case RasterKind::Block:
            return std::make_unique<BlockRasterizer>();
        case RasterKind::Sextant:
            return std::make_unique<SextantRasterizer>();
    }
    return nullptr;
}

std::unique_ptr<Rasterizer> RasterRegistry::create(std::string_view name) const {
    // 自定义登记优先：允许覆盖内置名字（实验用）。
    if (const auto it = custom_.find(name); it != custom_.end()) {
        return it->second();
    }
    if (name == "braille") {
        return create(RasterKind::Braille);
    }
    if (name == "halfblock") {
        return create(RasterKind::HalfBlock);
    }
    if (name == "quadrant") {
        return create(RasterKind::Quadrant);
    }
    if (name == "ascii") {
        return create(RasterKind::Ascii);
    }
    if (name == "block") {
        return create(RasterKind::Block);
    }
    if (name == "sextant") {
        return create(RasterKind::Sextant);
    }
    return nullptr;
}

bool RasterRegistry::supports(RasterKind kind) const noexcept {
    switch (kind) {
        case RasterKind::Braille:
        case RasterKind::HalfBlock:
        case RasterKind::Quadrant:
        case RasterKind::Ascii:
        case RasterKind::Block:
        case RasterKind::Sextant:
            return true;
    }
    return false;
}

const char* RasterRegistry::name(RasterKind kind) noexcept {
    return raster_name(kind);
}

}  // namespace termchart::raster
