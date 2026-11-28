// test_raster_block.cpp —— 八级竖条光栅器（1×1）
#include <doctest.h>

#include "termchart/media/image/field.hpp"
#include "termchart/raster/block.hpp"

namespace {

using namespace termchart;
using namespace termchart::raster;
using namespace termchart::image;

}  // namespace

TEST_CASE("raster/block: 子像素比 1×1，强度→八级竖条") {
    const BlockRasterizer r;
    CHECK(r.subpixel_width() == 1);
    CHECK(r.subpixel_height() == 1);

    FloatField f(1, 1, 0.0f);
    CellBuffer out = r.rasterize(f, RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u0020');  // 空格

    f.set(0, 0, 1.0f);
    out = r.rasterize(f, RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u2588');  // █ 全满

    f.set(0, 0, 0.5f);
    out = r.rasterize(f, RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u2584');  // ▄ 半满（round(0.5*8)=4）
}

TEST_CASE("raster/block: invert 反相强度") {
    const BlockRasterizer r;
    FloatField f(1, 1, 1.0f);
    const CellBuffer out = r.rasterize(
        f, RasterOptions{.fg = Color::ansi16(7), .bg = Color::ansi16(0), .invert = true});
    // 反相后强度变 0 → 空格
    CHECK(out.at(0, 0).cp == U'\u0020');
}
