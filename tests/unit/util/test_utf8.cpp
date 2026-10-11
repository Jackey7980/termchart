// test_utf8.cpp —— UTF-8 编解码与容错
//
// 容错契约：非法序列产出 U+FFFD 且**只消耗 1 字节**（游标一定前进，绝不死循环）。
#include <doctest.h>

#include <string>

#include "termchart/util/utf8.hpp"

namespace {

using namespace termchart::util;

TEST_CASE("util: 单字节与多字节往返") {
    CHECK(decode("A", 0).cp == U'A');
    CHECK(decode("A", 0).consumed == 1);
    CHECK(decode("A", 0).ok);

    const std::string cjk = "\u4e2d";
    const Utf8Decode cjk_result = decode(cjk, 0);
    CHECK(cjk_result.cp == U'\u4e2d');
    CHECK(cjk_result.consumed == 3);
    CHECK(cjk_result.ok);

    const std::string emoji = "\U0001F600";
    const Utf8Decode emoji_result = decode(emoji, 0);
    CHECK(emoji_result.cp == U'\U0001F600');
    CHECK(emoji_result.consumed == 4);
}

TEST_CASE("util: 编码后能解回原值") {
    for (const char32_t cp : {U'a', U'\u00e9', U'\u4e2d', U'\U0001F600'}) {
        const std::string encoded = to_utf8(cp);
        CHECK(decode(encoded, 0).cp == cp);
        CHECK(decode(encoded, 0).ok);
    }
}

TEST_CASE("util: 非法序列容错 —— 替换字符 + 只吃 1 字节") {
    SUBCASE("孤立续字节") {
        const std::string bad("\x80", 1);
        const Utf8Decode result = decode(bad, 0);
        CHECK_FALSE(result.ok);
        CHECK(result.cp == kReplacementChar);
        CHECK(result.consumed == 1);
    }
    SUBCASE("截断的三字节序列") {
        const std::string bad("\xe4\xb8", 2);
        const Utf8Decode result = decode(bad, 0);
        CHECK(result.cp == kReplacementChar);
        CHECK(result.incomplete);
        CHECK(result.consumed == 2);  // 把已有字节吃光，游标继续前进
    }
    SUBCASE("非法首字节 0xFF") {
        const std::string bad("\xff\xff", 2);
        const Utf8Decode result = decode(bad, 0);
        CHECK(result.cp == kReplacementChar);
        CHECK(result.consumed == 1);
    }
}

TEST_CASE("util: 超长编码与代理码点被拒") {
    // 超长编码：'/' 的 2 字节形式
    const std::string overlong("\xc0\xaf", 2);
    CHECK_FALSE(decode(overlong, 0).ok);
    CHECK(decode(overlong, 0).cp == kReplacementChar);

    // 代理码点 U+D800 不能出现在合法 UTF-8 中
    const std::string surrogate("\xed\xa0\x80", 3);
    CHECK_FALSE(decode(surrogate, 0).ok);
}

TEST_CASE("util: 遍历不会因非法输入而卡住") {
    // 用 static_cast<char> 拼字节：字面量里的 \xff 会触发"hex escape out of range"。
    const std::string mixed = std::string("a") + static_cast<char>(0xFF) +
                              static_cast<char>(0x80) + "b";
    std::size_t count = 0;
    std::size_t pos = 0;
    while (pos < mixed.size()) {
        const Utf8Decode result = decode(mixed, pos);
        REQUIRE(result.consumed > 0);  // 关键不变量
        pos += static_cast<std::size_t>(result.consumed);
        ++count;
        REQUIRE(count < 100);
    }
    CHECK(count == 4);
    CHECK(codepoint_count(mixed) == 4);
}

TEST_CASE("util: is_valid 严格判定") {
    CHECK(is_valid("hello"));
    CHECK(is_valid("\u4e2d\u6587"));
    CHECK_FALSE(is_valid(std::string("\x80", 1)));
    CHECK_FALSE(is_valid(std::string("a\xc0\xaf", 3)));
}

TEST_CASE("util: sanitize 产出合法 UTF-8") {
    const std::string dirty = std::string("a") + static_cast<char>(0x80) + "b";
    const auto cleaned = sanitize(dirty);
    REQUIRE(cleaned.has_value());
    CHECK(is_valid(*cleaned));
    CHECK(codepoint_count(*cleaned) == 3);
}

TEST_CASE("util: 越界位置返回 consumed == 0") {
    CHECK(decode("a", 1).consumed == 0);
    CHECK(decode("", 0).consumed == 0);
}

}  // namespace
