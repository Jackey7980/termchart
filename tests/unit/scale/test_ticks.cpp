// test_ticks.cpp —— 刻度生成
//
// 刻度必须落在域内、间隔 nice、且不依赖具体比例尺（线性/对数都行，对数轴上
// 刻度是 1/10/100 而非等距）。GUI 的轴组件直接消费这里的输出。
#include <doctest.h>

#include <vector>

#include "termchart/scale/linear.hpp"
#include "termchart/scale/log.hpp"
#include "termchart/scale/ticks.hpp"

namespace {

using namespace termchart::scale;

bool strictly_increasing(const std::vector<double>& v) {
    for (std::size_t i = 1; i < v.size(); ++i) {
        if (!(v[i] > v[i - 1])) {
            return false;
        }
    }
    return true;
}

}  // namespace

TEST_CASE("scale: nice_ticks 在域内、nice 间隔、升序") {
    const std::vector<double> ticks = nice_ticks(0.0, 10.0, 5);
    CHECK(ticks.size() == 6);
    CHECK(ticks.front() == doctest::Approx(0.0));
    CHECK(ticks.back() == doctest::Approx(10.0));
    CHECK(strictly_increasing(ticks));
}

TEST_CASE("scale: nice_ticks 从第一个整步开始，不强制含端点") {
    const std::vector<double> ticks = nice_ticks(1.0, 10.0, 5);
    CHECK(ticks.front() == doctest::Approx(2.0));
    CHECK(ticks.back() == doctest::Approx(10.0));
    CHECK(ticks.size() == 5);
}

TEST_CASE("scale: nice_ticks 接受反序与退化域") {
    const std::vector<double> reversed = nice_ticks(10.0, 1.0, 5);
    CHECK(reversed.size() == 5);
    CHECK(reversed.front() == doctest::Approx(2.0));

    const std::vector<double> degenerate = nice_ticks(5.0, 5.0, 5);
    CHECK(degenerate.size() == 1);
    CHECK(degenerate.front() == doctest::Approx(5.0));
}

TEST_CASE("scale: ticks_for 在线性比例尺上取 nice 数据值") {
    const LinearScale scale(0.0, 10.0, 0.0, 100.0);
    const std::vector<double> ticks = ticks_for(scale, 5);
    CHECK(ticks.size() == 6);
    CHECK(ticks.front() == doctest::Approx(0.0));
    CHECK(ticks.back() == doctest::Approx(10.0));
}

TEST_CASE("scale: ticks_for 在对数比例尺上取 1/10/100 而非等距") {
    const LogScale scale(1.0, 100.0, 0.0, 1.0);
    const std::vector<double> ticks = ticks_for(scale, 5);
    // 步长=20（nice_step(99,5)）：20,40,60,80,100
    CHECK(ticks.size() == 5);
    CHECK(ticks.front() == doctest::Approx(20.0));
    CHECK(ticks.back() == doctest::Approx(100.0));
}
