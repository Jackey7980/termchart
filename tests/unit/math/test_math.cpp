// test_math.cpp —— math 模块单测（stats / interpolate / fft / linalg / special）
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include <doctest.h>

#include "termchart/math/fft.hpp"
#include "termchart/math/interpolate.hpp"
#include "termchart/math/linalg.hpp"
#include "termchart/math/special.hpp"
#include "termchart/math/stats.hpp"

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

}  // TEST_SUITE
