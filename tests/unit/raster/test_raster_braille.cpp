// test_raster_braille.cpp —— 盲文光栅器的子像素几何与查表
#include <doctest.h>

#include "termchart/image/field.hpp"
#include "termchart/raster/braille.hpp"

namespace {

using namespace termchart;
using namespace termchart::raster;
using namespace termchart::image;

// 构造 field（2 列 × 4 行）并在指定子像素点亮。
FloatField dot_field(int col, int row) {
    FloatField f(2, 4, 0.0f);
    f.set(static_cast<std::uint32_t>(col), static_cast<std::uint32_t>(row), 1.0f);
    return f;
}

}  // namespace

// 水平桥接：一条只覆盖每格左半边的细横线，桥接后应是连续的一行点（不是虚线）。
// 回归：未桥接时两格各只亮 dot1 ⇒ 屏幕上长直线呈"一粒一粒"的虚线。
TEST_CASE("raster/braille: 水平桥接把细横线接成连续一行") {
    // 4×4 子像素 = 2 个盲文格；每格只点亮左列(row0) ⇒ 未桥接是 dot1。
    FloatField f(4, 4, 0.0f);
    f.set(0, 0, 1.0f);  // 第 0 格左列
    f.set(2, 0, 1.0f);  // 第 1 格左列

    const CellBuffer bridged = BrailleRasterizer{}.rasterize(f, RasterOptions{});
    // 桥接：两格都补上 dot4（右上）⇒ dot1|dot4 = 0x09 ⇒ U+2809
    CHECK(bridged.at(0, 0).cp == U'\u2809');
    CHECK(bridged.at(1, 0).cp == U'\u2809');

    // 关掉桥接 ⇒ 回到原始逐子像素判定（各亮一个点）
    const CellBuffer raw = BrailleRasterizer{}.rasterize(
        f, RasterOptions{.bridge_horizontal = false});
    CHECK(raw.at(0, 0).cp == U'\u2801');
    CHECK(raw.at(1, 0).cp == U'\u2801');
}

// 竖线不受桥接影响：只有一格亮 ⇒ 没有相邻格同行左点可配对。
TEST_CASE("raster/braille: 桥接不影响竖线与孤立点") {
    FloatField f(4, 4, 0.0f);
    f.set(0, 0, 1.0f);
    f.set(0, 1, 1.0f);
    f.set(0, 2, 1.0f);
    f.set(0, 3, 1.0f);
    const CellBuffer out = BrailleRasterizer{}.rasterize(f, RasterOptions{});
    // 左列四点全亮 = dot1|dot2|dot3|dot7 = 0x47 ⇒ U+2847
    CHECK(out.at(0, 0).cp == U'\u2847');
    CHECK(out.at(1, 0).cp == U'\u2800');  // 右边那格没东西
}

TEST_CASE("raster/braille: 子像素比 2×4，输出尺寸 = field/子像素") {
    const BrailleRasterizer r;
    CHECK(r.subpixel_width() == 2);
    CHECK(r.subpixel_height() == 4);
    CHECK(r.kind() == RasterKind::Braille);

    const FloatField f(4, 8, 0.0f);  // 2×2 个盲文格
    const CellBuffer out = r.rasterize(f, RasterOptions{});
    CHECK(out.width() == 2);
    CHECK(out.height() == 2);
}

TEST_CASE("raster/braille: 单点亮对应正确盲文字形") {
    const BrailleRasterizer r;
    // 左上 (col0,row0) → dot1 → U+2801
    CellBuffer out = r.rasterize(dot_field(0, 0), RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u2801');
    // 左下 (col0,row3) → dot7 → U+2840
    out = r.rasterize(dot_field(0, 3), RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u2840');
    // 右上 (col1,row0) → dot4 → U+2808
    out = r.rasterize(dot_field(1, 0), RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u2808');
    // 右下 (col1,row3) → dot8 → U+2802？ dot8=0x80 → U+2880
    out = r.rasterize(dot_field(1, 3), RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u2880');
}

TEST_CASE("raster/braille: 全亮 → 实心盲文 U+28FF") {
    FloatField f(2, 4, 1.0f);
    const CellBuffer out = BrailleRasterizer{}.rasterize(f, RasterOptions{});
    CHECK(out.at(0, 0).cp == U'\u28FF');
    // 点亮色落在 fg
    CHECK(out.at(0, 0).style.fg() == Color::ansi16(7));
}

TEST_CASE("raster/braille: invert 翻转点亮与颜色") {
    const FloatField f = dot_field(1, 0);  // 仅 dot4 亮
    const CellBuffer normal = BrailleRasterizer{}.rasterize(f, RasterOptions{});
    const CellBuffer inv = BrailleRasterizer{}.rasterize(
        f, RasterOptions{.fg = Color::ansi16(7), .bg = Color::ansi16(0), .invert = true});

    // 反相后原先的 dot4 熄灭，其余 7 点变亮 → U+28F7（除 dot4 外全亮）。
    CHECK(inv.at(0, 0).cp == U'\u28F7');
    CHECK(inv.at(0, 0).style.fg() == Color::ansi16(0));  // on 与 off 互换
    CHECK_FALSE(inv.at(0, 0).cp == normal.at(0, 0).cp);
}
