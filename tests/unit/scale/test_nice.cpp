// test_nice.cpp —— nice number 算法（刻度与值域对齐的核心）
//
// nice 数 = 1/2/5 × 10^k。它决定轴标签是否人类可读——这条性质将来 GUI 的
// 坐标轴控件也要复用，故单独成测。
#include <doctest.h>

#include "termchart/scale/nice.hpp"

namespace {

using namespace termchart::scale;
using doctest::Approx;

}  // namespace

TEST_CASE("scale: nice_number 收成 1/2/5×10^k") {
    CHECK(nice_number(0.17) == Approx(0.2));
    CHECK(nice_number(0.23) == Approx(0.5));  // 2.3 → 收成 5
    CHECK(nice_number(1.0) == Approx(1.0));
    CHECK(nice_number(2.0) == Approx(2.0));
    CHECK(nice_number(4.6) == Approx(5.0));
    CHECK(nice_number(7.3) == Approx(10.0));
    CHECK(nice_number(13.0) == Approx(20.0));  // 1.3 → 2
    CHECK(nice_number(95.0) == Approx(100.0));
    CHECK(nice_number(0.0) == 0.0);  // 非正无意义
}

TEST_CASE("scale: nice_number round=true 向上取整") {
    CHECK(nice_number(1.5, true) == Approx(2.0));
    CHECK(nice_number(4.6, true) == Approx(5.0));
    CHECK(nice_number(9.9, true) == Approx(10.0));
}

TEST_CASE("scale: nice_step 取 nice 上界步长") {
    CHECK(nice_step(95.0, 5) == Approx(20.0));
    CHECK(nice_step(1.0, 10) == Approx(0.1));
    CHECK(nice_step(-5.0, 5) == 0.0);   // 非正跨度
    CHECK(nice_step(10.0, 0) == 0.0);    // 零目标刻度数
}
