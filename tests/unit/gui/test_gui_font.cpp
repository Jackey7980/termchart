// test_gui_font.cpp —— gui::font（位图字体）契约
//
// 这层文本出口只承诺四件事：量宽 = 列数 × 格尺寸（列数与 util 显示宽度同口径）、
// ASCII 字形真的落了像素、缺字可见（占位框）且占格正确、画字不越界不崩溃。
// CJK 宽度判定与 UTF-8 容错由 util 层（wcwidth / utf8）自己的测试守住。
#include <doctest.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "termchart/core/point.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/gui/font.hpp"
#include "termchart/image/bitmap.hpp"

namespace {

using namespace termchart;
using termchart::gui::draw_text;
using termchart::gui::has_glyph;
using termchart::gui::measure_text;

constexpr double kCellW = 9.0;
constexpr double kCellH = 16.0;

// RGB 任一通道非零即前景（背景是全 0 位图）。
std::size_t foreground_count(const image::Bitmap& bitmap) {
    std::size_t count = 0;
    for (std::uint32_t y = 0; y < bitmap.height(); ++y) {
        for (std::uint32_t x = 0; x < bitmap.width(); ++x) {
            if (bitmap.at(x, y, 0) != 0 || bitmap.at(x, y, 1) != 0 ||
                bitmap.at(x, y, 2) != 0) {
                ++count;
            }
        }
    }
    return count;
}

}  // namespace

TEST_CASE("has_glyph: 内置 ASCII 真、其余假") {
    CHECK(has_glyph(U' '));      // 0x20：区间下界（空格字形是全 0，但"有字形"）
    CHECK(has_glyph(U'A'));
    CHECK(has_glyph(U'0'));
    CHECK(has_glyph(U'~'));      // 0x7E：区间上界
    CHECK_FALSE(has_glyph(U'\u4E2D'));  // 中（U+4E2D）：East Asian Wide，不在内置区间
    CHECK_FALSE(has_glyph(U'\u00E9'));  // é：Latin-1，超出内置区间
    CHECK_FALSE(has_glyph(U'\uFFFD'));
}

TEST_CASE("measure_text: 宽度 = 列数 × cell_w，高度 = cell_h") {
    const gui::TextMetrics a = measure_text("A", kCellW, kCellH);
    CHECK_EQ(a.width, doctest::Approx(kCellW));
    CHECK_EQ(a.height, doctest::Approx(kCellH));

    const gui::TextMetrics ab = measure_text("AB", kCellW, kCellH);
    CHECK_EQ(ab.width, doctest::Approx(2.0 * kCellW));
    CHECK_EQ(ab.height, doctest::Approx(kCellH));

    // 空串：0 宽但仍报一行高（行高与内容无关）。
    const gui::TextMetrics empty = measure_text("", kCellW, kCellH);
    CHECK_EQ(empty.width, doctest::Approx(0.0));
    CHECK_EQ(empty.height, doctest::Approx(kCellH));
}

TEST_CASE("measure_text: CJK 按显示宽度占 2 列") {
    const gui::TextMetrics m = measure_text("中", kCellW, kCellH);
    CHECK_EQ(m.width, doctest::Approx(2.0 * kCellW));
    CHECK_EQ(m.height, doctest::Approx(kCellH));
}

TEST_CASE("draw_text: ASCII 字形确实落了像素、且不是整格填充") {
    image::Bitmap bitmap(32, 32, 4);
    const double drawn =
        draw_text(bitmap, "A", Point{2.0, 2.0}, Rgb{255, 255, 255}, kCellW, kCellH);
    CHECK_EQ(drawn, doctest::Approx(kCellW));
    const std::size_t count = foreground_count(bitmap);
    CHECK(count > 10);   // 'A' 是 5×7 点阵里的 16 个点，空字形会得 0
    CHECK(count < 200);  // 整格填充是 6*8 起步的格面积，远大于字形点数
}

TEST_CASE("draw_text: 缺字 CJK 画占位框并按显示宽度返回") {
    image::Bitmap bitmap(48, 32, 4);
    const double drawn =
        draw_text(bitmap, "中", Point{4.0, 4.0}, Rgb{255, 255, 255}, kCellW, kCellH);
    CHECK_EQ(drawn, doctest::Approx(2.0 * kCellW));  // 返回宽度 = 2 列 × cell_w
    CHECK(foreground_count(bitmap) > 0);             // 占位框生效：不是静默空白
}

TEST_CASE("draw_text: 锚点在位图外不崩溃、返回值不受影响") {
    image::Bitmap bitmap(32, 32, 4);
    const double drawn =
        draw_text(bitmap, "AB", Point{-100.0, -100.0}, Rgb{255, 255, 255}, kCellW, kCellH);
    CHECK_EQ(drawn, doctest::Approx(2.0 * kCellW));  // 返回值只由文本与格尺寸决定
    CHECK_EQ(foreground_count(bitmap), 0);           // 全部像素被裁剪
}

TEST_CASE("draw_text: 轴刻度数字 1/2/3 的字形逐个非空") {
    for (const char* digit : {"1", "2", "3"}) {
        image::Bitmap bitmap(32, 32, 4);
        const double advance =
            draw_text(bitmap, digit, Point{2.0, 2.0}, Rgb{255, 255, 255}, kCellW, kCellH);
        CHECK_EQ(advance, doctest::Approx(kCellW));  // 单字符 advance = 1 格宽
        CHECK_MESSAGE(foreground_count(bitmap) > 5, "digit glyph is empty: ", digit);
    }
}
