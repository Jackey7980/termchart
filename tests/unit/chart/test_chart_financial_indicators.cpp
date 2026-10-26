// test_chart_financial_indicators.cpp —— MA / EMA / BOLL / MACD 纯数值验证
#include <doctest.h>

#include <cmath>
#include <limits>
#include <vector>

#include "termchart/chart/financial/indicators.hpp"

using namespace termchart::chart::financial;
using doctest::Approx;

namespace {
constexpr double kEps = 1e-9;
[[nodiscard]] bool is_nan(double v) noexcept { return std::isnan(v); }
}  // namespace

TEST_CASE("financial: SMA 滑动均值且前导窗口为 NaN") {
    const std::vector<double> v{1, 2, 3, 4, 5};
    const std::vector<double> s = sma(v, 3);
    REQUIRE(s.size() == 5);
    CHECK(is_nan(s[0]));
    CHECK(is_nan(s[1]));
    CHECK(s[2] == Approx(2.0).epsilon(kEps));    // (1+2+3)/3
    CHECK(s[3] == Approx(3.0).epsilon(kEps));    // (2+3+4)/3
    CHECK(s[4] == Approx(4.0).epsilon(kEps));    // (3+4+5)/3
}

TEST_CASE("financial: SMA window=1 恒等于原序列") {
    const std::vector<double> v{3.5, -1.0, 9.9, 0.0};
    const std::vector<double> s = sma(v, 1);
    for (std::size_t i = 0; i < v.size(); ++i) {
        CHECK(s[i] == Approx(v[i]).epsilon(kEps));
    }
}

TEST_CASE("financial: SMA 窗口超过长度 → 全 NaN") {
    const std::vector<double> v{1, 2, 3};
    const std::vector<double> s = sma(v, 10);
    for (double x : s) CHECK(is_nan(x));
}

TEST_CASE("financial: EMA alpha=2/(w+1) 递推正确") {
    const std::vector<double> v{1, 2, 3, 4, 5};
    const std::vector<double> e = ema(v, 2);  // alpha = 2/3
    REQUIRE(e.size() == 5);
    CHECK(e[0] == Approx(1.0).epsilon(kEps));
    CHECK(e[1] == Approx(5.0 / 3.0).epsilon(kEps));       // 2/3*2 + 1/3*1
    CHECK(e[2] == Approx(23.0 / 9.0).epsilon(kEps));      // 2/3*3 + 1/3*(5/3)
    CHECK(e[3] == Approx(95.0 / 27.0).epsilon(kEps));     // 2/3*4 + 1/3*(23/9)
    CHECK(e[4] == Approx(365.0 / 81.0).epsilon(kEps));    // 2/3*5 + 1/3*(95/27)
}

TEST_CASE("financial: EMA window=1 恒等于原序列") {
    const std::vector<double> v{3.5, -1.0, 9.9, 0.0};
    const std::vector<double> e = ema(v, 1);  // alpha = 1
    for (std::size_t i = 0; i < v.size(); ++i) {
        CHECK(e[i] == Approx(v[i]).epsilon(kEps));
    }
}

TEST_CASE("financial: EMA 遇到 NaN 保持已有状态") {
    const std::vector<double> v{1, 2, std::numeric_limits<double>::quiet_NaN(), 4};
    const std::vector<double> e = ema(v, 2);  // alpha=2/3
    // e[0]=1, e[1]=2/3*2+1/3*1=5/3, e[2]= 保持 5/3（NaN 不更新）, e[3]=2/3*4+1/3*(5/3)
    CHECK(e[0] == Approx(1.0).epsilon(kEps));
    CHECK(e[1] == Approx(5.0 / 3.0).epsilon(kEps));
    CHECK(e[2] == Approx(5.0 / 3.0).epsilon(kEps));
    CHECK(e[3] == Approx(2.0 / 3.0 * 4.0 + 1.0 / 3.0 * (5.0 / 3.0)).epsilon(kEps));
}

TEST_CASE("financial: BOLL 中轨=SMA、上下轨=中轨±k·标准差") {
    const std::vector<double> v{1, 2, 3, 4, 5};
    const BollingerBands b = bollinger(v, 3, 2.0);
    REQUIRE(b.mid.size() == 5);
    CHECK(is_nan(b.mid[0]));
    CHECK(is_nan(b.upper[0]));
    CHECK(is_nan(b.lower[0]));
    // 窗口 [1,2,3]: 均值 2, 标准差 sqrt(2/3) ≈ 0.8165
    const double sd = std::sqrt(2.0 / 3.0);
    CHECK(b.mid[2] == Approx(2.0).epsilon(kEps));
    CHECK(b.upper[2] == Approx(2.0 + 2.0 * sd).epsilon(kEps));
    CHECK(b.lower[2] == Approx(2.0 - 2.0 * sd).epsilon(kEps));
    CHECK(b.mid[4] == Approx(4.0).epsilon(kEps));
    CHECK(b.upper[4] == Approx(4.0 + 2.0 * sd).epsilon(kEps));
}

TEST_CASE("financial: MACD = EMA(fast)-EMA(slow)，且 macd[0]=0") {
    const std::vector<double> v{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    const Macd m = macd(v, 3, 6, 3);
    REQUIRE(m.macd.size() == v.size());
    CHECK(m.macd[0] == Approx(0.0).epsilon(kEps));  // 两端 EMA 初值均为 v[0]
    // 端到端核对：macd[i] = ema(v,3)[i] - ema(v,6)[i]
    const std::vector<double> ef = ema(v, 3);
    const std::vector<double> es = ema(v, 6);
    for (std::size_t i = 0; i < v.size(); ++i) {
        CHECK(m.macd[i] == Approx(ef[i] - es[i]).epsilon(kEps));
        CHECK(m.hist[i] == Approx(m.macd[i] - m.signal[i]).epsilon(kEps));
    }
    // signal 是 macd 的 EMA：从 index 0 起即有值，且 signal[0] == macd[0]。
    CHECK(m.signal[0] == Approx(m.macd[0]).epsilon(kEps));
    CHECK_FALSE(is_nan(m.signal[0]));
    CHECK_FALSE(is_nan(m.signal[1]));
}

TEST_CASE("financial: 空输入全部函数安全返回空/NaN 向量") {
    const std::vector<double> empty;
    CHECK(sma(empty, 3).empty());
    CHECK(ema(empty, 3).empty());
    const BollingerBands b = bollinger(empty, 3, 2.0);
    CHECK(b.mid.empty());
    const Macd m = macd(empty, 3, 6, 3);
    CHECK(m.macd.empty());
}
