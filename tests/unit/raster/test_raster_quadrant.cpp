// test_raster_quadrant.cpp —— 象限块光栅器（2×2）
#include <doctest.h>

#include "termchart/image/field.hpp"
#include "termchart/raster/quadrant.hpp"

namespace {

using namespace termchart;
using namespace termchart::raster;
using namespace termchart::image;

// 构造 2×2 field，按 (ul,ur,ll,lr) 设强度。
FloatField q_field(bool ul, bool ur, bool ll, bool lr) {
    FloatField f(2, 2, 0.0f);
    f.set(0, 0, ul ? 1.0f : 0.0f);  // UL
    f.set(1, 0, ur ? 1.0f : 0.0f);  // UR
    f.set(0, 1, ll ? 1.0f : 0.0f);  // LL
    f.set(1, 1, lr ? 1.0f : 0.0f);  // LR
    return f;
}

}  // namespace

TEST_CASE("raster/quadrant: 子像素比 2×2") {
    const QuadrantRasterizer r;
    CHECK(r.subpixel_width() == 2);
    CHECK(r.subpixel_height() == 2);
    const FloatField f(4, 4, 0.0f);  // 2×2 个象限格
    const CellBuffer out = r.rasterize(f, RasterOptions{});
    CHECK(out.width() == 2);
    CHECK(out.height() == 2);
}

TEST_CASE("raster/quadrant: 单/双点亮映射到正确象限字符") {
    const QuadrantRasterizer r;
    CHECK(r.rasterize(q_field(true, false, false, false), RasterOptions{}).at(0, 0).cp ==
          U'\u2598');  // UL
    CHECK(r.rasterize(q_field(false, true, false, false), RasterOptions{}).at(0, 0).cp ==
          U'\u259D');  // UR
    CHECK(r.rasterize(q_field(false, false, true, false), RasterOptions{}).at(0, 0).cp ==
          U'\u2596');  // LL
    CHECK(r.rasterize(q_field(false, false, false, true), RasterOptions{}).at(0, 0).cp ==
          U'\u2597');  // LR
    CHECK(r.rasterize(q_field(true, true, false, false), RasterOptions{}).at(0, 0).cp ==
          U'\u259B');  // 上（UL+UR）
    CHECK(r.rasterize(q_field(true, false, true, false), RasterOptions{}).at(0, 0).cp ==
          U'\u2599');  // 左（UL+LL）
    CHECK(r.rasterize(q_field(false, true, false, true), RasterOptions{}).at(0, 0).cp ==
          U'\u259C');  // 右（UR+LR）
    CHECK(r.rasterize(q_field(false, false, true, true), RasterOptions{}).at(0, 0).cp ==
          U'\u259F');  // 下（LL+LR）
}

TEST_CASE("raster/quadrant: 全亮 → 实心块，3 点亮经熄灭象限补偿") {
    const QuadrantRasterizer r;
    // 全亮
    CellBuffer out = r.rasterize(q_field(true, true, true, true), RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u2588');  // █

    // 仅 LR 熄灭（其余 3 点亮）：应画"唯一熄灭象限 LR" ▗ 为熄灭色，其余 fg
    out = r.rasterize(q_field(true, true, true, false), RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u2597');  // LR 象限字符
    CHECK(out.at(0, 0).style.fg() == Color::ansi16(0));  // 熄灭色在 fg
    CHECK(out.at(0, 0).style.bg() == Color::ansi16(7));  // 点亮色在 bg
}

TEST_CASE("raster/quadrant: invert 反相") {
    const QuadrantRasterizer r;
    const CellBuffer out = r.rasterize(
        q_field(true, false, false, false),
        RasterOptions{.fg = Color::ansi16(7), .bg = Color::ansi16(0), .invert = true});
    // 反相后 UL 熄灭、其余 3 点亮 → 画熄灭象限 UL ▘，该象限落在 swapped off 上。
    // invert 时 on/off 交换：off = 原 fg = ansi16(7)，故 fg 为 ansi16(7)。
    CHECK(out.at(0, 0).cp == U'\u2598');
    CHECK(out.at(0, 0).style.fg() == Color::ansi16(7));
    CHECK(out.at(0, 0).style.bg() == Color::ansi16(0));
}
