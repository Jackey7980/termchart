// test_raster_halfblock.cpp —— 半块光栅器（1×2，上下双色）
#include <doctest.h>

#include "termchart/media/image/field.hpp"
#include "termchart/raster/halfblock.hpp"

namespace {

using namespace termchart;
using namespace termchart::raster;
using namespace termchart::image;

// 构造 1 列 × 2 行的 field，设 top/bottom 强度。
FloatField hb_field(float top, float bot) {
    FloatField f(1, 2, 0.0f);
    f.set(0, 0, top);
    f.set(0, 1, bot);
    return f;
}

}  // namespace

TEST_CASE("raster/halfblock: 子像素比 1×2") {
    const HalfBlockRasterizer r;
    CHECK(r.subpixel_width() == 1);
    CHECK(r.subpixel_height() == 2);
    const FloatField f(3, 4, 0.0f);  // 3 列 × 2 行 → 3×2 个半块格
    const CellBuffer out = r.rasterize(f, RasterOptions{});
    CHECK(out.width() == 3);
    CHECK(out.height() == 2);
}

TEST_CASE("raster/halfblock: 上亮/下亮/全亮/全灭") {
    const HalfBlockRasterizer r;
    CellBuffer out = r.rasterize(hb_field(1.0f, 0.0f), RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u2580');  // ▀ 上半
    CHECK(out.at(0, 0).style.fg() == Color::ansi16(7));

    out = r.rasterize(hb_field(0.0f, 1.0f), RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u2584');  // ▄ 下半

    out = r.rasterize(hb_field(1.0f, 1.0f), RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u2588');  // █ 全亮

    out = r.rasterize(hb_field(0.0f, 0.0f), RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u0020');  // 空格
}

TEST_CASE("raster/halfblock: 阈值以下不点亮") {
    const HalfBlockRasterizer r;
    const CellBuffer out = r.rasterize(hb_field(0.4f, 0.4f), RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u0020');
}

TEST_CASE("raster/halfblock: invert 反相") {
    const HalfBlockRasterizer r;
    const CellBuffer out = r.rasterize(
        hb_field(1.0f, 0.0f),
        RasterOptions{.fg = Color::ansi16(7), .bg = Color::ansi16(0), .invert = true});
    // 反相后"上"变熄灭、"下"变亮 → ▄
    CHECK(out.at(0, 0).cp == U'\u2584');
    CHECK(out.at(0, 0).style.fg() == Color::ansi16(0));
}
