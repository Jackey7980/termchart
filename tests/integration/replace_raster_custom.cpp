// replace_raster_custom.cpp —— 可扩展性实验 C（ROADMAP 阶段 3 契约验证点）
//
// 命题：**新增一个光栅器不需要改本模块的枚举或 switch**——实现 raster::Rasterizer
// 接口，经 RasterRegistry::register_raster(name, creator) 登记即可。
//
// 注：ROADMAP 原命名 replace_raster_sextant；六分块字形表（U+1FB00+ 的 60 个
// 格局）依赖较新 Unicode 区块、易错，暂缓。本实验用同构的"4 级阴影"光栅器
// 验证**同一个**注册表扩展点。
#include <doctest.h>

#include <cstdint>
#include <memory>
#include <string>

#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/style.hpp"
#include "termchart/media/image/field.hpp"
#include "termchart/raster/rasterizer.hpp"
#include "termchart/raster/registry.hpp"

namespace {

using namespace termchart;

// 自定义光栅器：每格按强度取 4 级阴影之一。
class ShadeRasterizer final : public raster::Rasterizer {
public:
    CellBuffer rasterize(const image::FloatField& field,
                         const raster::RasterOptions& opts) const override {
        CellBuffer out(field.width(), field.height());
        const Color on = opts.invert ? opts.bg : opts.fg;
        const Color off = opts.invert ? opts.fg : opts.bg;
        for (std::uint32_t y = 0; y < field.height(); ++y) {
            for (std::uint32_t x = 0; x < field.width(); ++x) {
                double v = static_cast<double>(field.at(x, y));
                if (opts.invert) {
                    v = 1.0 - v;
                }
                char32_t cp = U' ';
                if (v >= 0.75) {
                    cp = U'\u2588';  // █
                } else if (v >= 0.5) {
                    cp = U'\u2593';  // ▓
                } else if (v >= 0.25) {
                    cp = U'\u2592';  // ▒
                } else if (v > 0.0) {
                    cp = U'\u2591';  // ░
                }
                out.at(x, y) = Cell{cp, Style{}.set_fg(on).set_bg(off)};
            }
        }
        return out;
    }

    // 说明：RasterKind 目前是封闭枚举，自定义光栅器借用一个语义最近的内置值。
    [[nodiscard]] raster::RasterKind kind() const noexcept override {
        return raster::RasterKind::Block;
    }
    [[nodiscard]] std::uint32_t subpixel_width() const noexcept override { return 1; }
    [[nodiscard]] std::uint32_t subpixel_height() const noexcept override { return 1; }
    [[nodiscard]] const char* name() const noexcept override { return "shade"; }
};

}  // namespace

TEST_CASE("replace/raster: 自定义光栅器经注册表接入，核心零改动") {
    raster::RasterRegistry& registry = raster::RasterRegistry::instance();
    registry.register_raster("shade", [] { return std::make_unique<ShadeRasterizer>(); });

    CHECK(registry.supports_name("shade"));
    std::unique_ptr<raster::Rasterizer> created = registry.create("shade");
    REQUIRE(created != nullptr);
    CHECK(std::string(created->name()) == "shade");

    image::FloatField field(2, 1);
    field.set(0, 0, 1.0f);
    field.set(1, 0, 0.3f);
    const CellBuffer out = created->rasterize(field, raster::RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u2588');  // █
    CHECK(out.at(1, 0).cp == U'\u2592');  // ▒
}
