// test_graph_junction.cpp —— 交汇口字符规则（表驱动，穷举 16 种方向组合）
//
// 这条规则原先有**三份**潜在复制（orgchart、UML 类图示例、地铁线路图），抽到
// `graph/route/junction` 之后只剩一份；表驱动穷举是为了让"以后谁改这张表"立刻可见。
#include <doctest.h>

#include <array>
#include <cstdint>

#include "termchart/graph/route/junction.hpp"

namespace {

using termchart::graph::junction_char;

struct Case {
    bool up;
    bool down;
    bool left;
    bool right;
    char32_t expected;
    const char* note;
};

constexpr char32_t kH = U'\u2500';
constexpr char32_t kV = U'\u2502';
constexpr char32_t kDR = U'\u250C';
constexpr char32_t kDL = U'\u2510';
constexpr char32_t kUR = U'\u2514';
constexpr char32_t kUL = U'\u2518';
constexpr char32_t kVR = U'\u251C';
constexpr char32_t kVL = U'\u2524';
constexpr char32_t kHD = U'\u252C';
constexpr char32_t kHU = U'\u2534';
constexpr char32_t kX = U'\u253C';

constexpr std::array<Case, 16> kTable{{
    {false, false, false, false, U' ', "零方向：不是交汇口"},
    {true, false, false, false, kV, "单方向（上）：线字符"},
    {false, true, false, false, kV, "单方向（下）：线字符"},
    {false, false, true, false, kH, "单方向（左）：线字符"},
    {false, false, false, true, kH, "单方向（右）：线字符"},
    {true, true, false, false, kV, "上下"},
    {false, false, true, true, kH, "左右"},
    {false, true, false, true, kDR, "┌ 下+右"},
    {false, true, true, false, kDL, "┐ 下+左"},
    {true, false, false, true, kUR, "└ 上+右"},
    {true, false, true, false, kUL, "┘ 上+左"},
    {true, true, false, true, kVR, "├ 上下右"},
    {true, true, true, false, kVL, "┤ 上下左"},
    {false, true, true, true, kHD, "┬ 左右下"},
    {true, false, true, true, kHU, "┴ 左右上"},
    {true, true, true, true, kX, "┼ 四向"},
}};

}  // namespace

TEST_CASE("graph/junction: 16 种方向组合逐一对应（表驱动）") {
    for (const Case& c : kTable) {
        CHECK(junction_char(c.up, c.down, c.left, c.right) == c.expected);
    }
}

TEST_CASE("graph/junction: 退化口径不返回方块字符") {
    // 单方向返回线字符（不是 T 形/十字），零方向返回空格：调用方据此跳过该格。
    CHECK(junction_char(false, false, true, false) == kH);
    CHECK(junction_char(true, false, false, false) == kV);
    CHECK(junction_char(false, false, false, false) == U' ');

    // 任何非零组合都不该返回空格（否则"本来有线的格"会被清掉）。
    for (const Case& c : kTable) {
        if (c.up || c.down || c.left || c.right) {
            CHECK(junction_char(c.up, c.down, c.left, c.right) != U' ');
        }
    }
}

TEST_CASE("graph/junction: 方向组合与字符的拓扑一致（不出现'缺了实际连通的那边'）") {
    // 这条是"观感 bug"的机器判据：把字符本身的连通性反解出来，和输入方向逐一比对，
    // 避免以后有人手滑把 ┌ 与 ┐、├ 与 ┤ 写反（这类错渲染时才看得出来）。
    for (const Case& c : kTable) {
        const char32_t cp = junction_char(c.up, c.down, c.left, c.right);
        bool has_up = false;
        bool has_down = false;
        bool has_left = false;
        bool has_right = false;
        switch (cp) {
            case U'\u2502':
                has_up = has_down = true;
                break;
            case U'\u2500':
                has_left = has_right = true;
                break;
            case U'\u250C':  // ┌ 下+右
                has_down = has_right = true;
                break;
            case U'\u2510':  // ┐ 下+左
                has_down = has_left = true;
                break;
            case U'\u2514':  // └ 上+右
                has_up = has_right = true;
                break;
            case U'\u2518':  // ┘ 上+左
                has_up = has_left = true;
                break;
            case U'\u251C':  // ├ 上下右
                has_up = has_down = has_right = true;
                break;
            case U'\u2524':  // ┤ 上下左
                has_up = has_down = has_left = true;
                break;
            case U'\u252C':  // ┬ 左右下
                has_left = has_right = has_down = true;
                break;
            case U'\u2534':  // ┴ 左右上
                has_left = has_right = has_up = true;
                break;
            case U'\u253C':  // ┼ 四向
                has_up = has_down = has_left = has_right = true;
                break;
            default:
                continue;  // 单方向/空格已在上一条用例覆盖
        }
        // 字符表达的连通性必须是输入方向的**超集**（单方向被归一到线字符时是严格超集）。
        if (c.up) {
            CHECK(has_up);
        }
        if (c.down) {
            CHECK(has_down);
        }
        if (c.left) {
            CHECK(has_left);
        }
        if (c.right) {
            CHECK(has_right);
        }
    }
}
