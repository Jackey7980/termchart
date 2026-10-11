// test_wcwidth.cpp —— 显示宽度模型
//
// 契约：**主码点决定宽度，侧面项宽 0**。
#include <doctest.h>

#include <string>

#include "termchart/core/grapheme.hpp"
#include "termchart/util/grapheme.hpp"
#include "termchart/util/wcwidth.hpp"

namespace {

using namespace termchart;
using namespace termchart::util;

TEST_CASE("util: 码点宽度分档") {
    CHECK(codepoint_width(U'a') == 1);
    CHECK(codepoint_width(U'\u4e2d') == 2);        // 东亚宽
    CHECK(codepoint_width(U'\u0301') == 0);        // 组合记号
    CHECK(codepoint_width(U'\u200B') == 0);        // 零宽空格
    CHECK(codepoint_width(U'\n') == 0);            // 控制字符
}

TEST_CASE("util: emoji 宽度受策略控制") {
    WidthOptions emoji_narrow;
    emoji_narrow.emoji_wide = false;
    WidthOptions emoji_wide;
    emoji_wide.emoji_wide = true;

    CHECK(codepoint_width(U'\U0001F600', emoji_wide) == 2);
    CHECK(codepoint_width(U'\U0001F600', emoji_narrow) == 1);
}

TEST_CASE("util: Ambiguous 宽度受策略控制") {
    WidthOptions wide_ambiguous;
    wide_ambiguous.ambiguous_wide = true;
    CHECK(codepoint_width(U'\u03b1', wide_ambiguous) == 2);  // 希腊字母
    CHECK(codepoint_width(U'\u03b1', WidthOptions{}) == 1);
}

TEST_CASE("util: 簇宽度等于主码点宽度") {
    GraphemeCluster cluster(U'e');
    CHECK(cluster_width(cluster) == 1);
    cluster.add_side_bearing(U'\u0301');  // e + 尖音符：仍然宽 1
    CHECK(cluster_width(cluster) == 1);

    GraphemeCluster wide(U'\u4e2d');
    CHECK(cluster_width(wide) == 2);
}

TEST_CASE("util: 整串宽度按簇累加") {
    CHECK(display_width("hello") == 5);
    CHECK(display_width("\u4e2d\u6587") == 4);
    CHECK(display_width("e\u0301e") == 2);  // 组合记号不占宽
}

TEST_CASE("util: 按宽度截断永不切在组合记号中间") {
    const std::string text = "a\u0301b";  // a+组合, b
    const std::size_t cut = prefix_by_width(text, 1);
    // 'a' 与它的组合记号必须同进同出：'a' 一字节 + U+0301 两字节 = 3 字节，显示宽 1。
    CHECK(cut == 3);
    CHECK(display_width(text.substr(0, cut)) == 1);

    CHECK(prefix_by_width(text, 0) == 0);
    CHECK(prefix_by_width("hello", 100) == 5);
}

}  // namespace
