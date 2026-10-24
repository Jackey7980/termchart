// test_math.cpp —— math 模块单测（stats / interpolate / fft / linalg / special）
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <span>
#include <vector>

#include <doctest.h>

#include "termchart/math/color_space.hpp"
#include "termchart/math/fft.hpp"
#include "termchart/math/interpolate.hpp"
#include "termchart/math/linalg.hpp"
#include "termchart/math/power.hpp"
#include "termchart/math/special.hpp"
#include "termchart/math/stats.hpp"

// 跨模块不变量测试需要 data::quartiles 作为对照真源
#include "termchart/data/aggregate.hpp"

using namespace termchart;
using namespace termchart::math;

// ── stats ──────────────────────────────────────────────────────────────────

TEST_SUITE("math") {

TEST_CASE("math::stats::mean skips NaN (Kahan)") {
    const std::vector<double> xs = {1.0, std::nan(""), 3.0};
    CHECK(mean(xs) == doctest::Approx(2.0));
    CHECK(std::isnan(mean(std::vector<double>{})));
    CHECK(std::isnan(mean(std::vector<double>{std::nan(""), std::nan("")})));
}

TEST_CASE("math::stats::median odd/even") {
    const std::vector<double> odd = {3.0, 1.0, 2.0};
    CHECK(median(odd) == doctest::Approx(2.0));
    const std::vector<double> even = {4.0, 1.0, 3.0, 2.0};
    CHECK(median(even) == doctest::Approx(2.5));
    const std::vector<double> with_nan = {1.0, std::nan(""), 5.0, 3.0};
    CHECK(median(with_nan) == doctest::Approx(3.0));
}

TEST_CASE("math::stats::variance ddof 口径") {
    const std::vector<double> xs = {1.0, 2.0, 3.0};
    CHECK(variance(xs) == doctest::Approx(2.0 / 3.0));         // 总体（numpy 口径）
    CHECK(variance(xs, 1.0) == doctest::Approx(1.0));          // 样本（pandas 口径）
    CHECK(stddev(xs) == doctest::Approx(std::sqrt(2.0 / 3.0)));
    // 有效样本 ≤ ddof → NaN
    CHECK(std::isnan(variance(std::vector<double>{7.0}, 1.0)));
}

TEST_CASE("math::stats::covariance 与 pearson") {
    const std::vector<double> xs = {1.0, 2.0, 3.0};
    const std::vector<double> ys = {2.0, 4.0, 6.0};
    CHECK(covariance(xs, ys) == doctest::Approx(4.0 / 3.0));
    CHECK(pearson(xs, ys) == doctest::Approx(1.0));
    // 反相关
    const std::vector<double> zs = {6.0, 4.0, 2.0};
    CHECK(pearson(xs, zs) == doctest::Approx(-1.0));
    // 常数序列零方差 → NaN（无定义）
    const std::vector<double> cs = {5.0, 5.0, 5.0};
    CHECK(std::isnan(pearson(xs, cs)));
    // 长度不等取较短者：仅取前两对 (1,2),(2,4)，mx=1.5, my=3 → cov = 0.5
    const std::vector<double> short_ys = {2.0, 4.0};
    CHECK(covariance(xs, short_ys) == doctest::Approx(0.5));
}

// ── interpolate ────────────────────────────────────────────────────────────

TEST_CASE("math::interpolate::linear_at") {
    const std::vector<double> xs = {0.0, 1.0, 2.0, 4.0};
    const std::vector<double> ys = {0.0, 10.0, 20.0, 40.0};
    CHECK(linear_at(xs, ys, 0.5) == doctest::Approx(5.0));
    CHECK(linear_at(xs, ys, 3.0) == doctest::Approx(30.0));
    // 越界钳位不外推
    CHECK(linear_at(xs, ys, -5.0) == doctest::Approx(0.0));
    CHECK(linear_at(xs, ys, 100.0) == doctest::Approx(40.0));
    // NaN 传播
    CHECK(std::isnan(linear_at(xs, ys, std::nan(""))));
    CHECK(std::isnan(linear_at(std::vector<double>{}, std::vector<double>{}, 1.0)));
}

TEST_CASE("math::interpolate::CubicSpline 过点与线性精确") {
    // 线性数据：二阶导全 0，样条必须逐点精确
    const std::vector<double> lx = {0.0, 1.0, 2.0, 3.0};
    const std::vector<double> ly = {0.0, 2.0, 4.0, 6.0};
    const CubicSpline lin(lx, ly);
    REQUIRE(lin.valid());
    CHECK(lin.at(1.5) == doctest::Approx(3.0));
    CHECK(lin.at(2.5) == doctest::Approx(5.0));
    // 越界钳位
    CHECK(lin.at(-1.0) == doctest::Approx(0.0));
    CHECK(lin.at(9.0) == doctest::Approx(6.0));
}

TEST_CASE("math::interpolate::CubicSpline 对称性与坏输入 fail-loud") {
    const std::vector<double> xs = {0.0, 1.0, 2.0};
    const std::vector<double> ys = {0.0, 1.0, 0.0};
    const CubicSpline s(xs, ys);
    REQUIRE(s.valid());
    CHECK(s.at(1.0) == doctest::Approx(1.0));  // 过数据点
    CHECK(s.at(0.5) == doctest::Approx(s.at(1.5)));  // 自然样条对称性
    // 坏输入不抛异常：valid() = false，at() 恒 NaN
    const CubicSpline dup(std::vector<double>{0.0, 0.0, 1.0}, std::vector<double>{0.0, 1.0, 2.0});
    CHECK_FALSE(dup.valid());
    CHECK(std::isnan(dup.at(0.5)));
    const CubicSpline one(std::vector<double>{1.0}, std::vector<double>{1.0});
    CHECK_FALSE(one.valid());
    const CubicSpline nan_in(std::vector<double>{0.0, 1.0}, std::vector<double>{0.0, std::nan("")});
    CHECK_FALSE(nan_in.valid());
}

// ── fft ────────────────────────────────────────────────────────────────────

TEST_CASE("math::fft::直流与单频") {
    // 直流：[1,1,1,1] → 仅 bin 0 = 4
    std::vector<double> re = {1.0, 1.0, 1.0, 1.0};
    std::vector<double> im = {0.0, 0.0, 0.0, 0.0};
    REQUIRE(fft(re, im));
    CHECK(re[0] == doctest::Approx(4.0));
    for (std::size_t i = 1; i < 4; ++i) {
        CHECK(std::fabs(re[i]) < 1e-12);
        CHECK(std::fabs(im[i]) < 1e-12);
    }
    // sin(2πk/8) → bin 1 幅度 -4i
    const std::size_t n = 8;
    std::vector<double> sr(n, 0.0);
    std::vector<double> si(n, 0.0);
    for (std::size_t k = 0; k < n; ++k) {
        sr[k] = std::sin(2.0 * 3.14159265358979323846 * static_cast<double>(k) / 8.0);
    }
    REQUIRE(fft(sr, si));
    CHECK(std::fabs(sr[1]) < 1e-12);
    CHECK(si[1] == doctest::Approx(-4.0));
    // 实信号的共轭对称 bin（N-1）为 +4i，不是 0
    CHECK(std::fabs(sr[n - 1]) < 1e-12);
    CHECK(si[n - 1] == doctest::Approx(4.0));
    for (std::size_t i = 2; i + 1 < n; ++i) {
        CHECK(std::fabs(sr[i]) < 1e-12);
        CHECK(std::fabs(si[i]) < 1e-12);
    }
}

TEST_CASE("math::fft::往返恒等与非 2 幂拒绝") {
    const std::vector<double> src = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    std::vector<double> re = src;
    std::vector<double> im(src.size(), 0.0);
    REQUIRE(fft(re, im));
    REQUIRE(fft(re, im, /*inverse=*/true));
    for (std::size_t i = 0; i < src.size(); ++i) {
        CHECK(re[i] == doctest::Approx(src[i]).epsilon(1e-12));
        CHECK(std::fabs(im[i]) < 1e-12);
    }
    // 非 2 幂：拒绝且不改数据
    std::vector<double> bad_re = {1.0, 2.0, 3.0};
    std::vector<double> bad_im = {0.0, 0.0, 0.0};
    CHECK_FALSE(fft(bad_re, bad_im));
    CHECK(bad_re[0] == 1.0);
    CHECK(bad_re[2] == 3.0);
    // 空 / 不等长
    std::vector<double> empty_re;
    std::vector<double> empty_im;
    CHECK_FALSE(fft(empty_re, empty_im));
    std::vector<double> mismatch_re(4, 0.0);
    std::vector<double> mismatch_im(2, 0.0);
    CHECK_FALSE(fft(mismatch_re, mismatch_im));
    CHECK(next_pow2(0) == 1);
    CHECK(next_pow2(5) == 8);
    CHECK(next_pow2(8) == 8);
}

// ── linalg ─────────────────────────────────────────────────────────────────

TEST_CASE("math::linalg::multiply/inverse/solve/determinant") {
    const std::vector<double> a = {4.0, 7.0, 2.0, 6.0};  // 行主序 2×2
    const double det = determinant(a, 2);
    CHECK(det == doctest::Approx(10.0));
    // A·A⁻¹ = I
    std::vector<double> inv(4, 0.0);
    REQUIRE(inverse(a, 2, inv));
    CHECK(inv[0] == doctest::Approx(0.6));
    CHECK(inv[1] == doctest::Approx(-0.7));
    CHECK(inv[2] == doctest::Approx(-0.2));
    CHECK(inv[3] == doctest::Approx(0.4));
    std::vector<double> prod(4, 0.0);
    REQUIRE(multiply(a, inv, 2, prod));
    CHECK(prod[0] == doctest::Approx(1.0));
    CHECK(prod[1] == doctest::Approx(0.0).epsilon(1e-12));
    CHECK(prod[2] == doctest::Approx(0.0).epsilon(1e-12));
    CHECK(prod[3] == doctest::Approx(1.0));
    // solve A x = [1,0] → x = A⁻¹ 第一列
    const std::vector<double> b = {1.0, 0.0};
    std::vector<double> x(2, 0.0);
    REQUIRE(solve(a, b, 2, x));
    CHECK(x[0] == doctest::Approx(0.6));
    CHECK(x[1] == doctest::Approx(-0.2));
    // 奇异矩阵 fail-loud
    const std::vector<double> sing = {1.0, 2.0, 2.0, 4.0};
    std::vector<double> junk(4, -1.0);
    CHECK_FALSE(inverse(sing, 2, junk));
    CHECK(determinant(sing, 2) == doctest::Approx(0.0).epsilon(1e-12));
    // 3×3 对角阵
    const std::vector<double> d3 = {2.0, 0.0, 0.0, 0.0, 4.0, 0.0, 0.0, 0.0, 5.0};
    std::vector<double> d3inv(9, 0.0);
    REQUIRE(inverse(d3, 3, d3inv));
    CHECK(d3inv[0] == doctest::Approx(0.5));
    CHECK(d3inv[4] == doctest::Approx(0.25));
    CHECK(d3inv[8] == doctest::Approx(0.2));
    CHECK(determinant(d3, 3) == doctest::Approx(40.0));
    // 尺寸不足 → false
    std::vector<double> tiny_out(3, 0.0);
    CHECK_FALSE(multiply(a, a, 2, tiny_out));
    std::vector<double> tiny_x(1, 0.0);
    CHECK_FALSE(solve(a, b, 2, tiny_x));
}

// ── special ────────────────────────────────────────────────────────────────

TEST_CASE("math::special::gamma/ln_gamma/erf") {
    CHECK(termchart::math::gamma(5.0) == doctest::Approx(24.0));
    CHECK(termchart::math::gamma(0.5) == doctest::Approx(std::sqrt(3.14159265358979323846)));
    CHECK(ln_gamma(5.0) == doctest::Approx(std::log(24.0)));
    // 极点显式 NaN（标准库这里回 ±Inf）
    CHECK(std::isnan(termchart::math::gamma(0.0)));
    CHECK(std::isnan(termchart::math::gamma(-3.0)));
    CHECK(std::isnan(ln_gamma(-1.0)));
    CHECK(termchart::math::erf(0.0) == doctest::Approx(0.0));
    CHECK(termchart::math::erf(1.0) == doctest::Approx(0.8427007929497149));
    CHECK(termchart::math::erfc(0.0) == doctest::Approx(1.0));
}

TEST_CASE("math::special::bessel 级数与渐近路径") {
    // 级数路径（|x| ≤ 20）：教科书常数
    CHECK(bessel_j0(1.0) == doctest::Approx(0.7651976865579666));
    CHECK(bessel_j1(1.0) == doctest::Approx(0.4400505857449335));
    // 级数路径的另两个教科书常数
    CHECK(bessel_j0(10.0) == doctest::Approx(-0.2459357644513483));
    CHECK(bessel_j1(10.0) == doctest::Approx(0.0434727461688614));
    // 渐近路径（|x| > 20）无独立常数可对，改用两条自洽检验：
    // ① 与级数路径在过渡区连续（公式写错会在这里差出量级）；
    const double j0_lo = bessel_j0(19.999);
    const double j0_hi = bessel_j0(20.001);
    CHECK(std::fabs(j0_hi - j0_lo) < 5e-4);
    const double j1_lo = bessel_j1(19.999);
    const double j1_hi = bessel_j1(20.001);
    CHECK(std::fabs(j1_hi - j1_lo) < 5e-4);
    // ② 振幅包络 |Jν(x)| ≲ sqrt(2/(πx))（Hankel 渐近的一阶包络）。
    CHECK(std::fabs(bessel_j0(25.0)) <= 1.05 * std::sqrt(2.0 / 3.14159265358979323846 / 25.0));
    CHECK(std::fabs(bessel_j1(40.0)) <= 1.05 * std::sqrt(2.0 / 3.14159265358979323846 / 40.0));
    // 负参数对称性
    CHECK(bessel_j0(-3.0) == doctest::Approx(bessel_j0(3.0)));
    CHECK(bessel_j1(-3.0) == doctest::Approx(-bessel_j1(3.0)));
    // 递推：J2(1) = 2·J1(1) - J0(1)
    CHECK(bessel_j(2, 1.0) == doctest::Approx(0.1149034849319005).epsilon(1e-10));
    CHECK(bessel_j(2, 1.0) ==
          doctest::Approx(2.0 * bessel_j1(1.0) - bessel_j0(1.0)).epsilon(1e-10));
    // 原点与 n=0/1 归口
    CHECK(bessel_j(0, 0.0) == doctest::Approx(1.0));
    CHECK(bessel_j(3, 0.0) == doctest::Approx(0.0));
    CHECK(bessel_j(-1, 1.0) == doctest::Approx(-bessel_j1(1.0)));
    // NaN 传播
    CHECK(std::isnan(bessel_j0(std::nan(""))));
}

// ── power / color_space ────────────────────────────────────────────────────

TEST_CASE("math::int_pow 快速幂与 INT_MIN 边界") {
    // 常量求值是最严的 UB 检查：有符号溢出会直接编译失败。
    static_assert(int_pow(2.0, 10) == 1024.0);
    static_assert(int_pow(2.0, 0) == 1.0);
    static_assert(int_pow(0.0, 0) == 1.0);
    static_assert(int_pow(2.0, -2) == 0.25);
    static_assert(int_pow(-2.0, 3) == -8.0);
    // 若直接对 exp 取反，INT_MIN 会踩有符号溢出 UB。这里只能运行时验：常量求值
    // 禁止浮点溢出到 inf（2^-2^31 下溢），但运行时必须安静返回 0 而不是崩。
    CHECK(int_pow(2.0, std::numeric_limits<int>::min()) == 0.0);
    // 用 std::pow 当参照（自己写常数容易写错位数）。容差 1e-9 而非 1e-15：快速幂
    // 每平方一次就把底数的表示误差放大 2 倍，log2(1e6)≈20 次后约 1e-11 量级。
    CHECK(int_pow(1.0000001, 1000000) ==
          doctest::Approx(std::pow(1.0000001, 1000000)).epsilon(1e-9));
    CHECK(std::isinf(int_pow(0.0, -1)));  // Approx 比不了 inf（inf/inf = NaN）
}

TEST_CASE("math::cbrt 与 std::cbrt 一致") {
    // 必须写全限定：<cmath> 在全局命名空间也提供 ::cbrt，配合 using namespace
    // 会让非限定调用二义。
    for (const double x : {1e-300, 1e-9, 0.125, 1.0, 2.0, 27.0, 1e9, 1e300}) {
        CHECK(termchart::math::cbrt(x) == doctest::Approx(std::cbrt(x)).epsilon(1e-12));
    }
    CHECK(termchart::math::cbrt(-8.0) == doctest::Approx(-2.0).epsilon(1e-12));
    CHECK(termchart::math::cbrt(0.0) == 0.0);
    CHECK(std::isinf(termchart::math::cbrt(std::numeric_limits<double>::infinity())));
    CHECK(std::isnan(termchart::math::cbrt(std::nan(""))));
    // 常量求值路径必须可用（这正是 std::cbrt 给不了的）
    constexpr double kCube = termchart::math::cbrt(27.0);
    static_assert(kCube > 2.999999999999 && kCube < 3.000000000001);
    // 大/小量级下 Newton 不发散（初值按指数域缩放的意义）
    CHECK(termchart::math::cbrt(1e-300) == doctest::Approx(1e-100).epsilon(1e-12));
}

TEST_CASE("math::srgb 电光转换端点与往返") {
    CHECK(srgb_to_linear(0.0) == 0.0);
    CHECK(srgb_to_linear(1.0) == doctest::Approx(1.0));
    CHECK(srgb_to_linear(0.5) == doctest::Approx(0.21404114048223255).epsilon(1e-12));
    // 分段点连续：漏掉 1.055/0.055 缩放会在这里断出 10% 的台阶
    CHECK(srgb_to_linear(0.04045) == doctest::Approx(0.0031308049535603713).epsilon(1e-9));
    CHECK(srgb_to_linear(0.040451) - srgb_to_linear(0.040449) < 1e-5);
    for (const double x : {0.0, 0.02, 0.3, 0.8, 1.0}) {
        CHECK(linear_to_srgb(srgb_to_linear(x)) == doctest::Approx(x).epsilon(1e-12));
    }
    // 8bit 表必须等于同一函数的 float 版——不是纯 x^2.4
    CHECK(srgb8_to_linear(128) ==
          doctest::Approx(static_cast<float>(srgb_to_linear(128.0 / 255.0))));
    CHECK(srgb8_to_linear(0) == 0.0f);
    CHECK(srgb8_to_linear(255) == doctest::Approx(1.0f));
    CHECK(linear_to_srgb8(0.0) == 0);
    CHECK(linear_to_srgb8(1.0) == 255);
    CHECK(linear_to_srgb8(srgb_to_linear(128.0 / 255.0)) == 128);
}

TEST_CASE("math::hsv 与 rgb 互转") {
    CHECK(hsv_to_rgb(0.0, 1.0, 1.0) == Rgb{255, 0, 0});
    CHECK(hsv_to_rgb(60.0, 1.0, 1.0) == Rgb{255, 255, 0});
    CHECK(hsv_to_rgb(120.0, 1.0, 1.0) == Rgb{0, 255, 0});
    CHECK(hsv_to_rgb(240.0, 1.0, 1.0) == Rgb{0, 0, 255});
    CHECK(hsv_to_rgb(300.0, 1.0, 1.0) == Rgb{255, 0, 255});
    CHECK(hsv_to_rgb(360.0, 1.0, 1.0) == Rgb{255, 0, 0});   // 循环回到 0
    CHECK(hsv_to_rgb(-120.0, 1.0, 1.0) == Rgb{0, 0, 255});  // 负角取模
    CHECK(hsv_to_rgb(200.0, 0.0, 0.5) == Rgb{128, 128, 128});  // 纯灰无色相
    CHECK(hsv_to_rgb(std::nan(""), 1.0, 1.0) == Rgb{255, 0, 0});  // 不可解释 → 红端
    // 往返：整数色必须原样回来（量化误差不得累积）
    for (const Rgb c : {Rgb{255, 0, 0}, Rgb{0, 255, 0}, Rgb{0, 0, 255}, Rgb{128, 128, 128},
                        Rgb{30, 144, 255}, Rgb{17, 34, 51}}) {
        double h = 0.0;
        double s = 0.0;
        double v = 0.0;
        rgb_to_hsv(c, h, s, v);
        CHECK(hsv_to_rgb(h, s, v) == c);
    }
}

TEST_CASE("math::lab 白点与往返") {
    double l = 0.0;
    double a = 0.0;
    double b = 0.0;
    rgb_to_lab(Rgb{0, 0, 0}, l, a, b);
    CHECK(std::fabs(l) < 1e-9);
    rgb_to_lab(Rgb{255, 255, 255}, l, a, b);
    CHECK(l == doctest::Approx(100.0).epsilon(1e-6));
    // 白点 (0.95047, 1, 1.08883) 与正反矩阵系数各自只有 7 位有效数字，
    // 残差落在 1e-5 量级是舍入噪声，不是换算出错（数量级差 4 位才是 bug）。
    CHECK(std::fabs(a) < 1e-4);
    CHECK(std::fabs(b) < 1e-4);
    // sRGB 红 (255,0,0) 在 D65 下的公开参考值
    rgb_to_lab(Rgb{255, 0, 0}, l, a, b);
    CHECK(l == doctest::Approx(53.2408).epsilon(1e-3));
    CHECK(a == doctest::Approx(80.0925).epsilon(1e-3));
    CHECK(b == doctest::Approx(67.2032).epsilon(1e-3));
    // 往返：8bit 量化允许 ±1
    for (const Rgb c : {Rgb{255, 0, 0}, Rgb{30, 144, 255}, Rgb{17, 34, 51}}) {
        rgb_to_lab(c, l, a, b);
        const Rgb back = lab_to_rgb(l, a, b);
        CHECK(std::abs(static_cast<int>(back.r) - static_cast<int>(c.r)) <= 1);
        CHECK(std::abs(static_cast<int>(back.g) - static_cast<int>(c.g)) <= 1);
        CHECK(std::abs(static_cast<int>(back.b) - static_cast<int>(c.b)) <= 1);
    }
}

// ── stats: 分位数 / 极值篱（batch 1） ────────────────────────────────────────
//
// 这三者是 AutoViz 稳健 Y 轴域与 chart KDE 带宽的地基。此前仓库里分位数有两份
// 独立实现（data::quartiles 走 sort + 线性插值、chart/statistical/distribution.cpp
// 里的匿名 quantile），口径相同却各写一遍——"同一数字两个真源"正是分叉起点。
// 现在以 math::stats 为唯一真源（线性插值法，pos = p·(n-1)），且 quantile(xs,0.5)
// 必须与 math::median 严格一致。

TEST_CASE("math::stats::quickselect 第 k 小 + 越界不 UB") {
    std::vector<double> v = {3, 1, 4, 1, 5, 9, 2, 6};
    std::vector<double> sorted = v;
    std::sort(sorted.begin(), sorted.end());  // {1,1,2,3,4,5,6,9}

    CHECK(std::isnan(quickselect(std::span<double>(v), 100)));  // 越界 → NaN，不越界
    for (std::size_t k = 0; k < v.size(); ++k) {
        std::vector<double> copy = v;
        CHECK(quickselect(std::span<double>(copy), k) == doctest::Approx(sorted[k]));
    }
    // 只部分排序：multiset 不变（排序后相等），且第 k 小仍正确
    std::vector<double> copy = v;
    const double kth = quickselect(std::span<double>(copy), 3);
    CHECK(kth == doctest::Approx(sorted[3]));
    std::sort(copy.begin(), copy.end());
    CHECK(copy == sorted);
}

TEST_CASE("math::stats::quantile 线性插值 + 边界") {
    const std::vector<double> odd = {1, 2, 3, 4, 5};
    CHECK(quantile(odd, 0.0) == doctest::Approx(1.0));
    CHECK(quantile(odd, 0.25) == doctest::Approx(2.0));
    CHECK(quantile(odd, 0.5) == doctest::Approx(3.0));
    CHECK(quantile(odd, 0.75) == doctest::Approx(4.0));
    CHECK(quantile(odd, 1.0) == doctest::Approx(5.0));

    const std::vector<double> even = {1, 2, 3, 4};
    CHECK(quantile(even, 0.5) == doctest::Approx(2.5));
    // quantile(0.5) 必须 == median——两条路径不能漂移
    CHECK(quantile(even, 0.5) == doctest::Approx(median(even)));

    // p 越界钳位；p 为 NaN → NaN
    CHECK(quantile(odd, -1.0) == doctest::Approx(1.0));
    CHECK(quantile(odd, 2.0) == doctest::Approx(5.0));
    CHECK(std::isnan(quantile(odd, std::nan(""))));

    // 空 / 全 NaN → NaN
    CHECK(std::isnan(quantile(std::vector<double>{}, 0.5)));
    CHECK(std::isnan(quantile(std::vector<double>{std::nan(""), std::nan("")}, 0.5)));

    // NaN 跳过
    const std::vector<double> withnan = {std::nan(""), 2, 4, 6, std::nan("")};
    CHECK(quantile(withnan, 0.5) == doctest::Approx(4.0));
}

TEST_CASE("math::stats::tukey_fence 标准 + 退化 + 全 NaN") {
    // Q1=3.25, Q3=7.75, IQR=4.5 → 篱 [−3.5, 14.5]，100 是离群
    const std::vector<double> xs = {1, 2, 3, 4, 5, 6, 7, 8, 9, 100};
    const TukeyFence f = tukey_fence(xs, 1.5);
    CHECK(f.valid);
    CHECK(f.q1 == doctest::Approx(3.25));
    CHECK(f.q3 == doctest::Approx(7.75));
    CHECK(f.median == doctest::Approx(5.5));
    CHECK(f.iqr == doctest::Approx(4.5));
    CHECK(f.lo == doctest::Approx(-3.5));
    CHECK(f.hi == doctest::Approx(14.5));
    CHECK(f.contains(5.0));
    CHECK_FALSE(f.contains(100.0));

    // k 可调：k=3 的"极端离群"篱更宽
    const TukeyFence g = tukey_fence(xs, 3.0);
    CHECK(g.lo == doctest::Approx(3.25 - 3.0 * 4.5));
    CHECK(g.hi == doctest::Approx(7.75 + 3.0 * 4.5));

    // IQR 退化（过半取值相同）：靠相对中位数的尺度撑开，绝不退化成"全判离群"
    const TukeyFence degen = tukey_fence(std::vector<double>{5, 5, 5, 5}, 1.5);
    CHECK(degen.valid);
    CHECK(degen.q1 == doctest::Approx(5.0));
    CHECK(degen.iqr == doctest::Approx(0.0));
    CHECK(degen.lo == doctest::Approx(5.0 - 1.5 * 5.0));
    CHECK(degen.hi == doctest::Approx(5.0 + 1.5 * 5.0));

    // 单点样本：仍 valid，篱靠退化分支
    const TukeyFence one = tukey_fence(std::vector<double>{42.0}, 1.5);
    CHECK(one.valid);
    CHECK(one.median == doctest::Approx(42.0));

    // 全 NaN / 空 → 无效
    CHECK_FALSE(tukey_fence(std::vector<double>{std::nan(""), std::nan("")}, 1.5).valid);
    CHECK_FALSE(tukey_fence(std::vector<double>{}, 1.5).valid);
}

TEST_CASE("math::stats::tukey_fence 与 data::quartiles 口径一致") {
    // 单一真源：math 的篱不得与 data 的四分位漂移
    const std::vector<double> xs = {2, 5, 7, 1, 9, 3, 8, 4, 6, 100};
    const TukeyFence f = tukey_fence(xs, 1.5);
    const data::Quartiles q = data::quartiles(data::DataView{xs.data(), xs.size()});
    CHECK(f.q1 == doctest::Approx(q.q1).epsilon(1e-9));
    CHECK(f.median == doctest::Approx(q.median).epsilon(1e-9));
    CHECK(f.q3 == doctest::Approx(q.q3).epsilon(1e-9));
}

// ── interpolate: 平滑曲线（batch 2） ───────────────────────────────────────────

TEST_CASE("math::interpolate::smooth_step 边界 + 单调") {
    // GLSL smoothstep：中点恰为 0.5
    CHECK(smooth_step(0.0, 1.0, 0.5) == doctest::Approx(0.5));
    CHECK(smooth_step(0.0, 1.0, 0.0) == doctest::Approx(0.0));
    CHECK(smooth_step(0.0, 1.0, 1.0) == doctest::Approx(1.0));
    // 越界钳位
    CHECK(smooth_step(0.0, 1.0, -1.0) == doctest::Approx(0.0));
    CHECK(smooth_step(0.0, 1.0, 2.0) == doctest::Approx(1.0));
    // 退化区间（edge0==edge1）→ 0，不除零
    CHECK(smooth_step(2.0, 2.0, 5.0) == doctest::Approx(0.0));
    // 单调不减（采样验证）
    double prev = -1.0;
    for (int i = 0; i <= 10; ++i) {
        const double v = smooth_step(0.0, 1.0, static_cast<double>(i) / 10.0);
        CHECK(v >= prev - 1e-12);
        prev = v;
    }
}

TEST_CASE("math::interpolate::catmull_rom 端点 + 线性精确 + 2D") {
    // 端点：t=0→p1，t=1→p2
    CHECK(catmull_rom(0.0, 1.0, 2.0, 3.0, 0.0) == doctest::Approx(1.0));
    CHECK(catmull_rom(0.0, 1.0, 2.0, 3.0, 1.0) == doctest::Approx(2.0));
    // 对线性数据精确还原（Catmull-Rom 对立方精确，故对直线也精确）
    CHECK(catmull_rom(0.0, 1.0, 2.0, 3.0, 0.5) == doctest::Approx(1.5));
    // 2D 形态：对角线上的点应落在对角线上
    const Point r = catmull_rom(Point{0, 0}, Point{1, 1}, Point{2, 2}, Point{3, 3}, 0.5);
    CHECK(r.x == doctest::Approx(1.5));
    CHECK(r.y == doctest::Approx(1.5));
    // 2D 端点
    const Point a = catmull_rom(Point{0, 0}, Point{1, 1}, Point{2, 2}, Point{3, 3}, 0.0);
    CHECK(a == Point{1, 1});
}

// ── linalg: 向量距离 / 余弦相似度（batch 3） ────────────────────────────────

TEST_CASE("math::linalg::euclidean_distance 基本 + 边界") {
    const std::vector<double> a = {0.0, 0.0};
    const std::vector<double> b = {3.0, 4.0};
    CHECK(euclidean_distance(a, b) == doctest::Approx(5.0));
    CHECK(euclidean_distance(a, a) == doctest::Approx(0.0));

    // 长度不等取较短者：第三个分量不参与
    const std::vector<double> c = {3.0, 4.0, 100.0};
    CHECK(euclidean_distance(a, c) == doctest::Approx(5.0));

    // 空 / 含 NaN → NaN
    CHECK(std::isnan(euclidean_distance(std::vector<double>{}, a)));
    const std::vector<double> withnan = {0.0, std::nan("")};
    CHECK(std::isnan(euclidean_distance(a, withnan)));
}

TEST_CASE("math::linalg::cosine_similarity 平行/正交/反向 + 零向量") {
    const std::vector<double> x = {1.0, 0.0};
    const std::vector<double> y = {0.0, 1.0};
    CHECK(cosine_similarity(x, x) == doctest::Approx(1.0));
    CHECK(cosine_similarity(x, y) == doctest::Approx(0.0));
    const std::vector<double> neg = {-1.0, 0.0};
    CHECK(cosine_similarity(x, neg) == doctest::Approx(-1.0));

    // 零向量无定义 → NaN；空 / 含 NaN → NaN
    const std::vector<double> zero = {0.0, 0.0};
    CHECK(std::isnan(cosine_similarity(x, zero)));
    CHECK(std::isnan(cosine_similarity(std::vector<double>{}, x)));
    const std::vector<double> withnan = {1.0, std::nan("")};
    CHECK(std::isnan(cosine_similarity(x, withnan)));
}

// ── stats: 协方差矩阵（batch 3） ─────────────────────────────────────────────

TEST_CASE("math::stats::covariance_matrix 对称 + 与 covariance 同口径") {
    // 2 维、4 个样本，行主序
    const std::vector<double> data = {
        1.0, 2.0,
        2.0, 4.0,
        3.0, 6.0,
        4.0, 8.0,
    };
    const std::size_t n = 4;
    const std::size_t d = 2;
    std::vector<double> cov(d * d, 0.0);
    REQUIRE(covariance_matrix(data, n, d, cov, 1.0));

    const std::vector<double> col0 = {1.0, 2.0, 3.0, 4.0};
    const std::vector<double> col1 = {2.0, 4.0, 6.0, 8.0};
    CHECK(cov[0] == doctest::Approx(covariance(col0, col0, 1.0)));
    CHECK(cov[3] == doctest::Approx(covariance(col1, col1, 1.0)));
    CHECK(cov[1] == doctest::Approx(covariance(col0, col1, 1.0)));
    CHECK(cov[1] == doctest::Approx(cov[2]));  // 对称
    CHECK(pearson(col0, col1) == doctest::Approx(1.0));
}

TEST_CASE("math::stats::covariance_matrix 边界 + listwise 删除") {
    const std::vector<double> data = {1.0, 2.0, 2.0, 4.0, 3.0, 6.0};
    std::vector<double> cov(4, 0.0);

    // out 太小 / d == 0 / n == 0 → false
    CHECK_FALSE(covariance_matrix(data, 3, 2, std::span<double>(cov.data(), 3), 0.0));
    CHECK_FALSE(covariance_matrix(data, 3, 0, cov, 0.0));
    CHECK_FALSE(covariance_matrix(data, 0, 2, cov, 0.0));

    // 某行含 NaN → 整行跳过：剩 (1,2) 与 (3,6)
    const std::vector<double> withnan = {1.0, 2.0, std::nan(""), 9.0, 3.0, 6.0};
    REQUIRE(covariance_matrix(withnan, 3, 2, cov, 0.0));
    CHECK(cov[0] == doctest::Approx(1.0));  // 方差 x
    CHECK(cov[3] == doctest::Approx(4.0));  // 方差 y
    CHECK(cov[1] == doctest::Approx(2.0));  // 协方差

    // 全部行无效 → false
    const std::vector<double> allbad = {std::nan(""), 1.0, 2.0, std::nan("")};
    CHECK_FALSE(covariance_matrix(allbad, 2, 2, cov, 0.0));

    // 有效行数 ≤ ddof → false
    const std::vector<double> one = {1.0, 2.0};
    CHECK_FALSE(covariance_matrix(one, 1, 2, cov, 1.0));
}

// ── interpolate: 贝塞尔（batch 3） ───────────────────────────────────────────

TEST_CASE("math::interpolate::quadratic_bezier 端点 + 中点 + 2D") {
    CHECK(quadratic_bezier(0.0, 1.0, 2.0, 0.0) == doctest::Approx(0.0));
    CHECK(quadratic_bezier(0.0, 1.0, 2.0, 1.0) == doctest::Approx(2.0));
    // t=0.5：0.25·0 + 0.5·1 + 0.25·2 = 1
    CHECK(quadratic_bezier(0.0, 1.0, 2.0, 0.5) == doctest::Approx(1.0));
    // 控制点在中点时退化为直线
    CHECK(quadratic_bezier(0.0, 0.5, 1.0, 0.25) == doctest::Approx(0.25));

    const Point m = quadratic_bezier(Point{0, 0}, Point{1, 2}, Point{2, 0}, 0.5);
    CHECK(m.x == doctest::Approx(1.0));
    CHECK(m.y == doctest::Approx(1.0));
}

TEST_CASE("math::interpolate::cubic_bezier 端点 + 中点 + 2D") {
    CHECK(cubic_bezier(0.0, 1.0, 2.0, 3.0, 0.0) == doctest::Approx(0.0));
    CHECK(cubic_bezier(0.0, 1.0, 2.0, 3.0, 1.0) == doctest::Approx(3.0));
    // 控制点取三等分点 → 直线，B(0.5) = 1.5
    CHECK(cubic_bezier(0.0, 1.0, 2.0, 3.0, 0.5) == doctest::Approx(1.5));

    const Point m = cubic_bezier(Point{0, 0}, Point{1, 1}, Point{2, 1}, Point{3, 0}, 0.5);
    CHECK(m.x == doctest::Approx(1.5));
    CHECK(m.y == doctest::Approx(0.75));
}

}  // TEST_SUITE
