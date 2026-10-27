// test_ttf_font_outline.cpp —— TtfFont::glyph_outline（文字轮廓化核心 API）单元测试。
//
// 用合成字体（'A' = 方形轮廓，(200,0)-(400,200)；' ' = 空；unitsPerEm = 1000，
// hmtx 步进 .notdef=500 / 'A'=600 / ' '=250），几何完全确定、不依赖真实字体文件。
#include <doctest.h>

#include <cstddef>
#include <cmath>
#include <vector>

#include "synthetic_font.hpp"
#include "termchart/core/expected.hpp"
#include "termchart/image/ttf_font.hpp"

namespace {

using namespace termchart;
using namespace termchart::image;
using termchart::testing::build_font;

bool all_finite(const std::vector<Point>& poly) {
    for (const auto& p : poly) {
        if (!(std::isfinite(p.x) && std::isfinite(p.y))) {
            return false;
        }
    }
    return true;
}

}  // namespace

TEST_CASE("ttf/glyph_outline: 'A' 返回像素空间闭合轮廓") {
    const auto font = TtfFont::load(build_font());
    REQUIRE(font.has_value());

    const auto go = font->glyph_outline(U'A', 1000.0);
    REQUIRE(go.has_value());
    CHECK(!go->contours.empty());
    CHECK(go->contours[0].size() >= 3);
    for (const auto& contour : go->contours) {
        CHECK(all_finite(contour));
    }
    // hmtx 'A' 步进 = 600，scale = 1000/1000 = 1 → advance_x == 600
    CHECK(go->advance_x == doctest::Approx(600.0).epsilon(1e-6));
    // 字形主体位于基线上方 → top（相对基线偏移）为负
    CHECK(go->top < 0);
    // 轮廓外接框左边界为正（字形原点在左侧 bearing 之后）
    CHECK(go->left > 0);
}

TEST_CASE("ttf/glyph_outline: 空格与缺字回退到轮廓为空、advance 仍有效") {
    const auto font = TtfFont::load(build_font());
    REQUIRE(font.has_value());

    const auto space = font->glyph_outline(U' ', 1000.0);
    REQUIRE(space.has_value());
    CHECK(space->contours.empty());
    CHECK(space->advance_x == doctest::Approx(250.0).epsilon(1e-6));

    // 未映射码点 → .notdef（glyph 0，空轮廓），advance = 500
    const auto missing = font->glyph_outline(U'\x4E2D' /* 中 */, 1000.0);
    REQUIRE(missing.has_value());
    CHECK(missing->contours.empty());
    CHECK(missing->advance_x == doctest::Approx(500.0).epsilon(1e-6));
}

TEST_CASE("ttf/glyph_outline: 像素尺寸缩放正确") {
    const auto font = TtfFont::load(build_font());
    REQUIRE(font.has_value());

    const auto small = font->glyph_outline(U'A', 100.0);   // scale = 0.1
    const auto large = font->glyph_outline(U'A', 2000.0);  // scale = 2.0
    REQUIRE(small.has_value());
    REQUIRE(large.has_value());
    CHECK(small->advance_x == doctest::Approx(60.0).epsilon(1e-6));
    CHECK(large->advance_x == doctest::Approx(1200.0).epsilon(1e-6));
    // 轮廓点也按 scale 缩放：large 的 x 范围应是 small 的 20 倍
    double small_min_x = small->contours[0][0].x;
    double large_min_x = large->contours[0][0].x;
    for (const auto& p : small->contours[0]) small_min_x = std::min(small_min_x, p.x);
    for (const auto& p : large->contours[0]) large_min_x = std::min(large_min_x, p.x);
    CHECK(large_min_x / small_min_x == doctest::Approx(20.0).epsilon(1e-3));
}
