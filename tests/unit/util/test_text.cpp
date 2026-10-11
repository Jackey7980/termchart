// test_text.cpp —— 文本排版（以显示宽度为单位）
#include <doctest.h>

#include "termchart/util/text.hpp"
#include "termchart/util/wcwidth.hpp"

namespace {

using namespace termchart::util;

TEST_CASE("util: 重复字符") {
    CHECK(repeat(U'-', 5) == "-----");
    CHECK(repeat(U'\u2500', 3) == "\u2500\u2500\u2500");
    CHECK(repeat(U'x', 0).empty());
}

TEST_CASE("util: 对齐填充") {
    CHECK(pad("ab", 5) == "ab   ");                       // 默认左对齐
    CHECK(pad("ab", 5, Align::Right) == "   ab");
    CHECK(pad("ab", 6, Align::Center) == "  ab  ");
    CHECK(pad("\u4e2d", 4, Align::Left) == "\u4e2d  ");   // 汉字算 2 宽
}

TEST_CASE("util: 截断与省略符") {
    CHECK(truncate("hello", 10) == "hello");              // 不够长则原样
    CHECK(truncate("hello", 3).size() <= 4);              // 放省略符
    CHECK(display_width(truncate("hello", 5)) <= 5);

    SUBCASE("宽度不足时退化为省略符") {
        const std::string cut = truncate("hello", 2, Align::Left, "..");
        CHECK(display_width(cut) <= 2);
    }
}

TEST_CASE("util: 折行按簇边界断开") {
    const auto lines = wrap("abcdef", 3);
    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "abc");
    CHECK(lines[1] == "def");

    SUBCASE("东亚宽字符按宽度折行") {
        const auto wide = wrap("\u4e2d\u6587\u4e2d\u6587", 4);
        REQUIRE(wide.size() == 2);
        CHECK(display_width(wide[0]) == 4);
    }
}

TEST_CASE("util: fit 同时做截断与补齐") {
    const std::string fitted = fit("hello", 3);
    CHECK(display_width(fitted) == 3);
}

}  // namespace
