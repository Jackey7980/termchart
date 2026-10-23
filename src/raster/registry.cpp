// registry.cpp —— 光栅器注册表实现（内置 + 自定义登记）
//
// 内置种类的唯一真源是 kBuiltinKinds + raster_name()：supports_name /
// create(name) / names() 全部从这张表推导，不再各自硬编码名字串
// （历史上三处各写一份，漂移过）。
#include "termchart/raster/registry.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/raster/ascii.hpp"
#include "termchart/raster/block.hpp"
#include "termchart/raster/braille.hpp"
#include "termchart/raster/halfblock.hpp"
#include "termchart/raster/quadrant.hpp"
#include "termchart/raster/sextant.hpp"

namespace termchart::raster {
namespace {

// 内置种类全表。新增内置光栅器 = 加一行 + create(kind) 加一个 case，
// 名字解析自动跟着走（raster_name 是名字真源）。
constexpr RasterKind kBuiltinKinds[] = {
    RasterKind::Braille, RasterKind::HalfBlock, RasterKind::Quadrant,
    RasterKind::Ascii,   RasterKind::Block,     RasterKind::Sextant,
};

[[nodiscard]] std::optional<RasterKind> builtin_kind_of(std::string_view name) noexcept {
    for (const RasterKind kind : kBuiltinKinds) {
        if (name == raster_name(kind)) {
            return kind;
        }
    }
    return std::nullopt;
}

}  // namespace

RasterRegistry& RasterRegistry::instance() noexcept {
    static RasterRegistry registry;
    return registry;
}

void RasterRegistry::register_raster(std::string_view name, Creator creator) {
    custom_[std::string(name)] = std::move(creator);
}

bool RasterRegistry::supports_name(std::string_view name) const {
    return custom_.find(name) != custom_.end() || builtin_kind_of(name).has_value();
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
    if (const auto kind = builtin_kind_of(name)) {
        return create(*kind);
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

std::vector<std::string> RasterRegistry::names() const {
    std::vector<std::string> out;
    out.reserve(std::size(kBuiltinKinds) + custom_.size());
    for (const RasterKind kind : kBuiltinKinds) {
        out.emplace_back(raster_name(kind));
    }
    for (const auto& [name, creator] : custom_) {
        if (std::find(out.begin(), out.end(), name) == out.end()) {
            out.push_back(name);  // 自定义不重复计内置名（允许覆盖，枚举去重）
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::size_t RasterRegistry::size() const noexcept {
    std::size_t count = std::size(kBuiltinKinds);
    for (const auto& [name, creator] : custom_) {
        if (builtin_kind_of(name).has_value()) {
            continue;  // 覆盖内置名的自定义不增加计数
        }
        ++count;
    }
    return count;
}

bool RasterRegistry::empty() const noexcept {
    return size() == 0;  // 恒 false（六个内置），保持六表一致接口
}

}  // namespace termchart::raster
