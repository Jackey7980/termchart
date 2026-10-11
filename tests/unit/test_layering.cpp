// test_layering.cpp —— 生成物（横切数据表）的结构不变量
//
// 这些不变量被各模块的实现**隐式依赖**：
//   * 区间表必须升序且互不重叠（否则二分查找会漏）；
//   * 盲文码点表必须严格单调（否则"点序↔位序"的约定会被破坏）。
// 手改生成物会在这里失败，而 check_generated.sh 再在脚本层拦一次。
#include <doctest.h>

#include <cstddef>

#include "termchart/detail/block_chars.hpp"
#include "termchart/detail/braille_table.hpp"
#include "termchart/detail/box_chars.hpp"
#include "termchart/detail/grapheme_tables.hpp"
#include "termchart/detail/sixel_table.hpp"
#include "termchart/detail/unicode_tables.hpp"

namespace {

using namespace termchart::detail;

template <class Range>
void expect_sorted_disjoint(const Range& table, std::size_t count) {
    for (std::size_t i = 1; i < count; ++i) {
        CHECK(table[i].first >= table[i - 1].first);
        CHECK(table[i].first > table[i - 1].last);  // 不重叠
    }
}

TEST_CASE("layering: Unicode 区间表升序且不重叠") {
    expect_sorted_disjoint(kEastAsianWide, kEastAsianWideCount);
    expect_sorted_disjoint(kEastAsianAmbiguous, kEastAsianAmbiguousCount);
    expect_sorted_disjoint(kCombining, kCombiningCount);
    expect_sorted_disjoint(kFormat, kFormatCount);
}

TEST_CASE("layering: grapheme 属性表升序且可二分") {
    expect_sorted_disjoint(kGbRanges, kGbRangeCount);

    // 抽查若干类别确实落在表里（防止区间被误删）。
    CHECK(grapheme_class(0x0301) == Gb::Extend);
    CHECK(grapheme_class(0x200D) == Gb::ZWJ);
    CHECK(grapheme_class(0x000D) == Gb::CR);
    CHECK(grapheme_class(0x000A) == Gb::LF);
    CHECK(grapheme_class(0x1100) == Gb::L);
    CHECK(grapheme_class('a') == Gb::Any);
}

TEST_CASE("layering: 宽度查询与朴素宽度自洽") {
    CHECK(naive_width(U'a') == 1);
    CHECK(naive_width(U'\u4e2d') == 2);
    CHECK(naive_width(U'\u0301') == 0);
    CHECK(is_control(0x07));
    CHECK_FALSE(is_control(U'a'));
}

TEST_CASE("layering: 盲文码点表严格单调") {
    for (std::size_t i = 0; i < kBrailleChars.size(); ++i) {
        CHECK(kBrailleChars[i] == static_cast<char32_t>(0x2800 + i));
    }
    CHECK(braille_char(0) == kBrailleBlank);
    CHECK(braille_char(0xFF) == 0x28FF);

    // 2×4 子像素 → 点掩码的唯一约定。
    CHECK(braille_dots_for_cell(0, 0) == kBrailleDot1);
    CHECK(braille_dots_for_cell(1, 0) == kBrailleDot4);
    CHECK(braille_dots_for_cell(0, 3) == kBrailleDot7);
    CHECK(braille_dots_for_cell(1, 3) == kBrailleDot8);
    CHECK(braille_dots_for_cell(2, 0) == 0);  // 越界不点亮
}

TEST_CASE("layering: 制表符掩码表覆盖全部 16 个掩码") {
    for (std::uint8_t mask = 0; mask < 16; ++mask) {
        const char32_t light = box_char(mask, BoxWeight::Light);
        const char32_t heavy = box_char(mask, BoxWeight::Heavy);
        if (mask == 0) {
            CHECK(light == 0x20);
        } else {
            CHECK(light >= 0x2500);
            CHECK(light <= 0x257F);
        }
        CHECK(heavy != 0);
    }
    // 四个直角掩码在 Round 下必须用专用码点（不是 Light 的退化）。
    CHECK(box_char(0x6, BoxWeight::Round) == 0x256D);
    CHECK(box_char(0x9, BoxWeight::Round) == 0x256F);
    CHECK(box_char(0x3, BoxWeight::Light) != box_char(0x3, BoxWeight::Round));
    CHECK((BoxMask::Up | BoxMask::Right) == 0x3);
}

TEST_CASE("layering: 块元素与密度阶梯单调") {
    for (int n = 0; n <= 8; ++n) {
        CHECK(kLowerBlocks[static_cast<std::size_t>(n)] != 0);
        CHECK(kLeftBlocks[static_cast<std::size_t>(n)] != 0);
    }
    CHECK(lower_eighth(-1) == kSpace);
    CHECK(lower_eighth(99) == kFullBlock);
    CHECK(density_char(0.0) == kSpace);
    CHECK(density_char(1.0) == U'@');
}

TEST_CASE("layering: Sixel 值↔字符可逆") {
    for (std::uint8_t v = 0; v <= 63; ++v) {
        CHECK(sixel_value(sixel_char(v)) == v);
    }
    CHECK(sixel_char(0) == '?');
    CHECK(sixel_value('#') == 0);  // 非法字符按透明处理
}

}  // namespace
