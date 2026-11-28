// test_raster_ascii.cpp —— 密度字符光栅器（1×1）
#include <doctest.h>

#include "termchart/media/image/field.hpp"
#include "termchart/raster/ascii.hpp"

namespace {

using namespace termchart;
using namespace termchart::raster;
using namespace termchart::image;

}  // namespace

TEST_CASE("raster/ascii: 强度→密度阶梯，端点正确") {
    const AsciiRasterizer r;
    CHECK(r.subpixel_width() == 1);
    CHECK(r.subpixel_height() == 1);

    FloatField f(1, 1, 0.0f);
    CellBuffer out = r.rasterize(f, RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u0020');  // 空格

    f.set(0, 0, 1.0f);
    out = r.rasterize(f, RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u0040');  // '@' 最浓

    f.set(0, 0, 0.5f);
    out = r.rasterize(f, RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u002B');  // '+'
}

TEST_CASE("raster/ascii: invert 反相强度") {
    const AsciiRasterizer r;
    FloatField f(1, 1, 1.0f);
    const CellBuffer out = r.rasterize(
        f, RasterOptions{.fg = Color::ansi16(7), .invert = true});
    CHECK(out.at(0, 0).cp == U'\u0020');  // 反相后强度为 0 → 空格
}
