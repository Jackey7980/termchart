// test_scale.cpp —— 线性 / 对数比例尺的映射与可逆性
//
// 比例尺是 chart / graph / canvas 的共享基础：GUI 控件也要用它把数据域映射到
// 像素。核心不变量是 forward / inverse 互逆（在 valid 域内），且越界输入走夹紧
// 或 NaN 而不崩——数据来自用户 CSV，含 NaN/非正数。
#include <doctest.h>

#include <cmath>
#include <limits>

#include "termchart/scale/linear.hpp"
#include "termchart/scale/log.hpp"
#include "termchart/scale/symlog.hpp"
#include "termchart/scale/registry.hpp"

namespace {

using namespace termchart;
using namespace termchart::scale;
using doctest::Approx;

}  // namespace

TEST_CASE("scale: 线性映射正方向正确") {
    const LinearScale scale(0.0, 10.0, 0.0, 100.0);
    CHECK(scale.forward(0.0) == Approx(0.0));
    CHECK(scale.forward(5.0) == Approx(50.0));
    CHECK(scale.forward(10.0) == Approx(100.0));
    CHECK(scale.valid());
}

TEST_CASE("scale: 线性 inverse 是 forward 的逆（域内）") {
    const LinearScale scale(0.0, 10.0, 0.0, 100.0);
    for (double d = 0.0; d <= 10.0; d += 1.0) {
        CHECK(scale.inverse(scale.forward(d)) == Approx(d));
    }
}

TEST_CASE("scale: 默认夹紧，越界不越界") {
    const LinearScale scale(0.0, 10.0, 0.0, 100.0, /*clamp=*/true);
    CHECK(scale.forward(-5.0) == Approx(0.0));
    CHECK(scale.forward(15.0) == Approx(100.0));
    CHECK(scale.inverse(-10.0) == Approx(0.0));
    CHECK(scale.inverse(110.0) == Approx(10.0));
}

TEST_CASE("scale: 关闭夹紧，越界线性外推") {
    const LinearScale scale(0.0, 10.0, 0.0, 100.0, /*clamp=*/false);
    CHECK(scale.forward(-5.0) == Approx(-50.0));
    CHECK(scale.inverse(150.0) == Approx(15.0));
}

TEST_CASE("scale: 退化域不合法，forward 退化为单点") {
    const LinearScale scale(5.0, 5.0, 0.0, 100.0);
    CHECK_FALSE(scale.valid());
    CHECK(scale.forward(999.0) == Approx(0.0));
    CHECK(scale.inverse(50.0) == Approx(5.0));
}

TEST_CASE("scale: 默认构造是单位映射，合法") {
    const LinearScale scale;
    CHECK(scale.valid());
    CHECK(scale.forward(0.5) == Approx(0.5));
    CHECK(scale.forward(1.0) == Approx(1.0));
}

TEST_CASE("scale: 对数映射在十的整数次幂处对齐") {
    const LogScale scale(1.0, 100.0, 0.0, 1.0, 10.0, true);
    CHECK(scale.forward(1.0) == Approx(0.0));
    CHECK(scale.forward(10.0) == Approx(0.5));
    CHECK(scale.forward(100.0) == Approx(1.0));
    CHECK(scale.inverse(0.5) == Approx(10.0));
    CHECK(scale.valid());
}

TEST_CASE("scale: 对数 inverse 逆回数据域") {
    const LogScale scale(1.0, 1000.0, 0.0, 1.0, 10.0, true);
    for (double v = 1.0; v <= 1000.0; v *= 2.0) {
        CHECK(scale.inverse(scale.forward(v)) == Approx(v).epsilon(1e-9));
    }
}

TEST_CASE("scale: 对数域含非正数则无效，forward 返回 NaN") {
    const LogScale bad(0.0, 10.0, 0.0, 1.0);
    CHECK_FALSE(bad.valid());
    CHECK(std::isnan(bad.forward(5.0)));

    const LogScale ok(1.0, 10.0, 0.0, 1.0);
    CHECK(ok.valid());
    CHECK(std::isnan(ok.forward(0.0)));
    CHECK(std::isnan(ok.forward(-3.0)));
    CHECK(std::isnan(ok.forward(std::numeric_limits<double>::quiet_NaN())));
}

TEST_CASE("scale: 任意底（自然对数）也能映射") {
    const LogScale scale(1.0, std::exp(1.0), 0.0, 1.0, std::exp(1.0), true);
    CHECK(scale.forward(std::exp(1.0)) == Approx(1.0));
    CHECK(scale.inverse(1.0) == Approx(std::exp(1.0)).epsilon(1e-9));
}

TEST_CASE("scale: 对称对数在零附近线性、远端对数") {
    // domain [-100,100] over [0,1]，linthresh=1：0 落在中点。
    const SymLogScale scale(-100.0, 100.0, 0.0, 1.0, 1.0, true);
    CHECK(scale.valid());
    CHECK(scale.forward(0.0) == Approx(0.5));
    // 对称：±x 关于 0.5 镜像。
    CHECK(scale.forward(-10.0) == Approx(1.0 - scale.forward(10.0)));
    // 越界夹紧到端点。
    CHECK(scale.forward(-1e9) == Approx(0.0));
    CHECK(scale.forward(1e9) == Approx(1.0));
}

TEST_CASE("scale: 对称对数 forward/inverse 域内互逆") {
    const SymLogScale scale(-1000.0, 1000.0, 0.0, 1.0, 2.0, true);
    for (double v = -900.0; v <= 900.0; v += 137.0) {
        CHECK(scale.inverse(scale.forward(v)) == Approx(v).epsilon(1e-9));
    }
}

TEST_CASE("scale: 对称对数退化/非正数 linthresh 无效") {
    const SymLogScale bad(-10.0, 10.0, 0.0, 1.0, 0.0, true);
    CHECK_FALSE(bad.valid());
    const SymLogScale ok(-10.0, 10.0, 0.0, 1.0, 1.0, true);
    CHECK(ok.valid());
    CHECK(std::isnan(ok.forward(std::numeric_limits<double>::quiet_NaN())));
}

TEST_CASE("scale: 注册表能按名创建 symlog") {
    auto s = ScaleRegistry::instance().create("symlog");
    CHECK(s != nullptr);
    CHECK(ScaleRegistry::instance().supports("symlog"));
}

