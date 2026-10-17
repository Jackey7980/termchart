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

TEST_CASE("util: 超宽单簇不被丢弃（雷区2）") {
    // 单个簇（如 2 宽的汉字）比行宽还宽时，force_break 必须整簇保留，
    // 绝不能静默丢字（旧实现 prefix_by_width 返回 0 → 整簇被吞）。
    const auto lines = wrap("\u4e2d", 1, true);
    REQUIRE(lines.size() == 1);
    CHECK(lines[0] == "\u4e2d");

    SUBCASE("超宽簇夹在文本中间也不丢后续字符") {
        const auto mixed = wrap("a\u4e2db", 1, true);
        std::string joined;
        for (const auto& l : mixed) {
            joined += l;
        }
        CHECK(joined == "a\u4e2db");  // 字符一个不少
    }
}

}  // namespace
