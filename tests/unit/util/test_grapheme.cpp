// test_grapheme.cpp —— grapheme cluster 分割（UAX #29 简化版）
//
// 分割是全库**唯一**做文本切分的地方：宽度、截断、折行、Cell 填充全部依赖它。
// 规则集见 util/grapheme.hpp；属性表见 detail/grapheme_tables.hpp（生成物）。
// 这里逐条把 GB3–GB13 钉住：改分割算法或重生成属性表都会在这里先失败。
#include <doctest.h>

#include <string>
#include <string_view>
#include <vector>

#include "termchart/core/grapheme.hpp"
#include "termchart/util/grapheme.hpp"

namespace {

using namespace termchart;
using namespace termchart::util;

std::vector<std::string> split_to_strings(std::string_view text) {
    std::vector<std::string> out;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t step = next_boundary(text, pos);
        REQUIRE(step > 0);  // 永不卡住
        out.emplace_back(text.substr(pos, step));
        pos += step;
    }
    return out;
}

std::string bad_byte_string() { return std::string("a") + static_cast<char>(0xFF); }

}  // namespace

TEST_CASE("util: ASCII 一码点一簇") {
    CHECK(cluster_count("hello") == 5);
    const std::vector<std::string> parts = split_to_strings("hello");
    REQUIRE(parts.size() == 5);
    CHECK(parts[0] == "h");
    CHECK(parts[4] == "o");
}

TEST_CASE("util: 组合记号并入主码点（GB9 Extend）") {
    CHECK(cluster_count("e\u0301") == 1);
    CHECK(cluster_count("e\u0301x") == 2);
    CHECK(cluster_count("\u0301") == 1);  // 孤立组合记号自成一簇
}

TEST_CASE("util: CRLF 是一簇（GB3），控制符自成一簇（GB4/GB5）") {
    CHECK(cluster_count("a\r\nb") == 3);
    const std::vector<std::string> parts = split_to_strings("a\r\nb");
    REQUIRE(parts.size() == 3);
    CHECK(parts[1] == "\r\n");
}

TEST_CASE("util: Hangul 初声·中声·终声连缀（GB6–GB8）") {
    CHECK(cluster_count("\u1100\u1161") == 1);          // L + V
    CHECK(cluster_count("\u1100\u1161\u11A8") == 1);    // L + V + T
    CHECK(cluster_count("\uAC00\u11A8") == 1);          // LV + T
    CHECK(cluster_count("\uAC01\u11A8") == 1);          // LVT + T
    CHECK(cluster_count("\uAC00\uAC01") == 2);          // 两个完整音节
}

TEST_CASE("util: ZWJ 连接的 emoji 是一簇（GB11）") {
    CHECK(is_extended_pictographic(U'\U0001F600'));
    CHECK_FALSE(is_extended_pictographic(U'a'));
    CHECK_FALSE(is_extended_pictographic(U'\u4e2d'));

    CHECK(cluster_count("\U0001F468\u200D\U0001F469") == 1);  // man ZWJ woman
    CHECK(cluster_count("\U0001F468\U0001F469") == 2);        // 无 ZWJ：两簇
}

TEST_CASE("util: 地区旗标两两连缀（GB12 / GB13）") {
    CHECK(cluster_count("\U0001F1FA\U0001F1F8") == 1);                  // 一对 = 一面旗
    CHECK(cluster_count("\U0001F1FA\U0001F1F8\U0001F1EB") == 2);        // 三连 = 1 + 1
    CHECK(cluster_count("\U0001F1FA\U0001F1F8\U0001F1EB\U0001F1F7") == 2);
}

TEST_CASE("util: SpacingMark 并入前一簇（GB9a）") {
    CHECK(cluster_count("\u0915\u093F") == 1);
}

TEST_CASE("util: Prepend 挂到后一簇（GB9b）") {
    CHECK(cluster_count("\u0600\u0627") == 1);
}

TEST_CASE("util: 东亚宽字符一码点一簇") {
    CHECK(cluster_count("\u4e2d\u6587") == 2);
    CHECK(cluster_count("\u4e2d\u0301\u6587") == 2);
}

TEST_CASE("util: next_boundary 在末尾返回 0，非法字节也保证前进") {
    CHECK(next_boundary("", 0) == 0);
    CHECK(next_boundary("abc", 3) == 0);
    CHECK(next_boundary("abc", 99) == 0);  // 越界不越界访问

    const std::string bad = bad_byte_string();
    CHECK(next_boundary(bad, 1) >= 1);
    CHECK(cluster_count(bad) == 2);  // 非法字节按替换字符自成一簇
}

TEST_CASE("util: split_clusters 与流式遍历等价") {
    const std::string text = "a\u0301\u4e2d\U0001F468\u200D\U0001F469z";
    const std::vector<GraphemeCluster> clusters = split_clusters(text);
    CHECK(clusters.size() == cluster_count(text));
    REQUIRE(clusters.size() == 4);
    CHECK(clusters.front().base() == U'a');
    CHECK(clusters.back().base() == U'z');
}
