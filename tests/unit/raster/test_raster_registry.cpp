// test_raster_registry.cpp —— 注册表与自动选择
#include <doctest.h>

#include <memory>

#include "termchart/raster/auto_select.hpp"
#include "termchart/raster/registry.hpp"

namespace {

using namespace termchart::raster;

}  // namespace

TEST_CASE("raster/registry: instance 单例、按 kind/名字创建") {
    RasterRegistry& reg = RasterRegistry::instance();
    CHECK(&reg == &RasterRegistry::instance());

    auto braille = reg.create(RasterKind::Braille);
    CHECK(braille != nullptr);
    CHECK(braille->kind() == RasterKind::Braille);
    CHECK(std::string(reg.name(RasterKind::Braille)) == "braille");

    auto by_name = reg.create("quadrant");
    CHECK(by_name != nullptr);
    CHECK(by_name->kind() == RasterKind::Quadrant);

    CHECK(reg.create("nope") == nullptr);
    CHECK(reg.create(static_cast<RasterKind>(-1)) == nullptr);
    CHECK(reg.supports(RasterKind::Block));
}

TEST_CASE("raster/auto_select: 首选支持则用首选") {
    RasterCaps caps{.braille = true, .halfblock = false, .quadrant = true,
                    .block = true, .ascii = true};
    CHECK(pick_raster(caps, RasterKind::Braille) == RasterKind::Braille);
    CHECK(pick_raster(caps, RasterKind::Quadrant) == RasterKind::Quadrant);
}

TEST_CASE("raster/auto_select: 首选不支持则按保真度降级") {
    // 盲文与象限都不支持，半块支持 → 降到半块
    RasterCaps caps{.braille = false, .halfblock = true, .quadrant = false,
                    .block = true, .ascii = true};
    CHECK(pick_raster(caps, RasterKind::Braille) == RasterKind::HalfBlock);
    // 都支持半块以上时，盲文优先于半块
    RasterCaps all{.braille = true, .halfblock = true, .quadrant = true,
                   .block = true, .ascii = true};
    CHECK(pick_raster(all, RasterKind::HalfBlock) == RasterKind::HalfBlock);
}

TEST_CASE("raster/auto_select: 全不支持兜底 ascii") {
    RasterCaps caps{};  // 全 false
    CHECK(pick_raster(caps, RasterKind::Braille) == RasterKind::Ascii);
}
