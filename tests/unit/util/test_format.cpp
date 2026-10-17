// test_format.cpp —— 数值格式化（format_double / format_int / format_compact）
//
// 锁住两条契约：① 普通/非有限值的格式稳定；② 极端量级（如 1e300）绝不返回空白
// （旧实现 kBufferSize=64 时 fixed+12 位小数约 313 字符 → value_too_large → 空串，
// 坐标轴标签会变空白，见雷区1）。
#include <doctest.h>

#include <cmath>
#include <limits>
#include <string>

#include "termchart/util/format.hpp"

namespace {

using namespace termchart::util;

TEST_CASE("util: 普通浮点格式不变") {
    CHECK(format_double(1.5, 2) == "1.50");
    CHECK(format_double(0.0, 2) == "0.00");
    CHECK(format_double(-2.0, 1) == "-2.0");
    CHECK(format_double(100.0, 0) == "100");
}

TEST_CASE("util: 非有限值") {
    CHECK(format_double(std::nan("")) == "nan");
    CHECK(format_double(std::numeric_limits<double>::infinity()) == "inf");
    CHECK(format_double(-std::numeric_limits<double>::infinity()) == "-inf");
}

TEST_CASE("util: 极端量级不返回空白（雷区1）") {
    // 放大缓冲区后，对任意有限 double 都必须非空；512 缓冲区足够容纳 fixed 表示
    // （double 上限约 1.8e308 → 309 位整数 + 12 位小数 < 512）。
    const std::string big = format_double(1e300, 2);
    CHECK_FALSE(big.empty());
    CHECK(big.size() > 300);
    CHECK_FALSE(format_double(1e308, 2).empty());

    // append_double 同样不丢。
    std::string acc;
    append_double(acc, 1e300, 2);
    CHECK_FALSE(acc.empty());

    // format_double_trimmed 也非空。
    CHECK_FALSE(format_double_trimmed(1e300, 2).empty());
}

TEST_CASE("util: format_compact 带单位") {
    CHECK(format_compact(1500.0, 1) == "1.5k");
    CHECK(format_compact(2.0e6, 0) == "2M");
}

TEST_CASE("util: format_int") {
    CHECK(format_int(0) == "0");
    CHECK(format_int(12345) == "12345");
    CHECK(format_int(-7) == "-7");
}

}  // namespace
