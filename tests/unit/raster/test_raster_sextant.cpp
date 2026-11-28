// test_raster_sextant.cpp —— 六分仪光栅器（2×3 子像素，U+1FB00 区）
//
// sextant 是 raster 里唯一零测试的头（ascii / block / braille / halfblock /
// quadrant / registry 都有用例）。这里钉住三件事：子像素契约、6 位掩码的位序、
// 与注册表的接线。
#include <doctest.h>

#include <cstdint>

#include "termchart/media/image/field.hpp"
#include "termchart/raster/registry.hpp"
#include "termchart/raster/sextant.hpp"

namespace {

using namespace termchart;
using namespace termchart::raster;
using namespace termchart::image;

// 位序约定（与 src/raster/sextant.cpp 一致）：左上 0 · 左中 1 · 左下 2 · 右上 3 · 右中 4 · 右下 5。
constexpr int kBit[2][3] = {{0, 1, 2}, {3, 4, 5}};

// 用一个 6 位掩码造 2×3 的场：置位的子像素为 1.0，其余 0.0。
FloatField field_from_mask(int mask) {
    FloatField field(2, 3, 0.0f);
    for (int col = 0; col < 2; ++col) {
        for (int row = 0; row < 3; ++row) {
            field.set(static_cast<std::uint32_t>(col), static_cast<std::uint32_t>(row),
                      (mask & (1 << kBit[col][row])) != 0 ? 1.0f : 0.0f);
        }
    }
    return field;
}

}  // namespace

TEST_CASE("raster/sextant: 子像素契约 2×3，输出尺寸 = 场 / (2,3)") {
    const SextantRasterizer r;
    CHECK(r.subpixel_width() == 2);
    CHECK(r.subpixel_height() == 3);
    CHECK(r.kind() == RasterKind::Sextant);
    CHECK(std::string(r.name()) == "sextant");

    const FloatField field(4, 6, 0.0f);  // 2×2 个字符格
    const CellBuffer out = r.rasterize(field, RasterOptions{});
    CHECK(out.width() == 2);
    CHECK(out.height() == 2);
}

TEST_CASE("raster/sextant: 单点掩码落在正确的六分仪位") {
    const SextantRasterizer r;
    for (int col = 0; col < 2; ++col) {
        for (int row = 0; row < 3; ++row) {
            const int mask = 1 << kBit[col][row];
            const CellBuffer out = r.rasterize(field_from_mask(mask), RasterOptions{});
            CHECK(out.at(0, 0).cp == static_cast<char32_t>(0x1FB00 + mask));
        }
    }
}

TEST_CASE("raster/sextant: 全空为空格、全满为实心块") {
    const SextantRasterizer r;
    CHECK(r.rasterize(field_from_mask(0), RasterOptions{}).at(0, 0).cp == U' ');
    CHECK(r.rasterize(field_from_mask(0x3F), RasterOptions{}).at(0, 0).cp == U'\u2588');

    // 5 点亮（仅右下熄灭）也走码点分支，不是实心块。
    const CellBuffer out = r.rasterize(field_from_mask(0x3F & ~(1 << 5)), RasterOptions{});
    CHECK(out.at(0, 0).cp == static_cast<char32_t>(0x1FB00 + 0x1F));
}

TEST_CASE("raster/sextant: 点亮色在 fg，invert 反相") {
    const SextantRasterizer r;
    const CellBuffer lit = r.rasterize(field_from_mask(0x3F), RasterOptions{});
    CHECK(lit.at(0, 0).style.fg() == Color::ansi16(7));
    CHECK(lit.at(0, 0).style.bg() == Color::ansi16(0));

    // invert：空场全部点亮 → 实心块，且点亮色换成 bg。
    const CellBuffer inverted =
        r.rasterize(field_from_mask(0), RasterOptions{.invert = true});
    CHECK(inverted.at(0, 0).cp == U'\u2588');
    CHECK(inverted.at(0, 0).style.fg() == Color::ansi16(0));
}

TEST_CASE("raster/sextant: 注册表按种类与名字都能取到") {
    const RasterRegistry& registry = RasterRegistry::instance();
    CHECK(registry.supports(RasterKind::Sextant));
    CHECK(std::string(RasterRegistry::name(RasterKind::Sextant)) == "sextant");

    const std::unique_ptr<Rasterizer> by_kind = registry.create(RasterKind::Sextant);
    REQUIRE(by_kind != nullptr);
    CHECK(std::string(by_kind->name()) == "sextant");
    CHECK(by_kind->subpixel_height() == 3);

    CHECK(registry.supports_name("sextant"));
    const std::unique_ptr<Rasterizer> by_name = registry.create(std::string_view("sextant"));
    REQUIRE(by_name != nullptr);
    CHECK(by_name->kind() == RasterKind::Sextant);
}
