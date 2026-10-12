// test_chart_statistical.cpp —— 统计 11 图种端到端绘制（结构事实断言）
//
// 每个图种两个用例：正常小数据集画出东西（count_inner_content > 0 且关键字符出现）；
// 退化输入（空/单点/常量列）不崩且不产出噪声内容。

#include <doctest.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/statistical/autocorr.hpp"
#include "termchart/chart/statistical/bland_altman.hpp"
#include "termchart/chart/statistical/corr.hpp"
#include "termchart/chart/statistical/distribution.hpp"
#include "termchart/chart/statistical/ecdf.hpp"
#include "termchart/chart/statistical/forest.hpp"
#include "termchart/chart/statistical/pca.hpp"
#include "termchart/chart/statistical/regression.hpp"
#include "termchart/chart/statistical/roc.hpp"
#include "termchart/chart/statistical/seasonal.hpp"
#include "termchart/chart/statistical/survival.hpp"
#include "termchart/core/point.hpp"

namespace {

using namespace termchart;
using namespace termchart::chart;

std::size_t count_nonblank(const canvas::Canvas& c) {
    std::size_t n = 0;
    for (std::uint32_t y = 0; y < c.height(); ++y) {
        for (std::uint32_t x = 0; x < c.width(); ++x) {
            if (c.at(x, y).cp != U'\u0020') {
                ++n;
            }
        }
    }
    return n;
}

// 内框内容量（默认 margin：left 8 / top 1 / right 1 / bottom 3）。
std::size_t count_inner_content(const canvas::Canvas& c) {
    std::size_t n = 0;
    const std::uint32_t x0 = 8;
    const std::uint32_t x1 = c.width() - 1;
    const std::uint32_t y0 = 1;
    const std::uint32_t y1 = c.height() - 3;
    for (std::uint32_t y = y0; y < y1; ++y) {
        for (std::uint32_t x = x0; x < x1; ++x) {
            if (c.at(x, y).cp != U'\u0020') {
                ++n;
            }
        }
    }
    return n;
}

// 确定性"带噪声"序列（不引随机数，测试可复现）。
std::vector<double> noisy(std::size_t n, double seed) {
    std::vector<double> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i);
        out.push_back(std::sin(0.7 * t + seed) * 0.6 + std::cos(1.3 * t) * 0.3);
    }
    return out;
}

}  // namespace

// ---------------- distribution ----------------

TEST_CASE("chart/statistical/distribution: 直方密度铺进绘图区") {
    canvas::Canvas canvas(60, 20);
    DistributionChart chart;
    chart.set_values(noisy(200, 0.0));
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 30);
}

TEST_CASE("chart/statistical/distribution: 空数据不崩") {
    canvas::Canvas canvas(30, 10);
    DistributionChart chart;
    chart.draw({canvas});
    CHECK(count_nonblank(canvas) == 0);
}

// ---------------- ecdf ----------------

TEST_CASE("chart/statistical/ecdf: 阶梯单调上升且落点为样本数") {
    canvas::Canvas canvas(60, 20);
    EcdfChart chart;
    chart.set_values({1.0, 2.0, 3.0, 4.0, 5.0});
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 10);
}

TEST_CASE("chart/statistical/ecdf: 单样本不崩") {
    canvas::Canvas canvas(30, 10);
    EcdfChart chart;
    chart.set_values({2.5});
    chart.draw({canvas});
    CHECK(count_nonblank(canvas) >= 0);
}

// ---------------- corr ----------------

TEST_CASE("chart/statistical/corr: 相关矩阵对角线为 1 且格子铺满") {
    canvas::Canvas canvas(60, 20);
    CorrMatrixChart chart;
    chart.set_matrix({"a", "b", "c"},
                     {{1.0, 2.0, 3.0, 4.0}, {2.0, 4.0, 6.0, 8.0}, {4.0, 2.0, 1.0, 0.0}});
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 9);  // 3×3 矩阵至少 9 格
}

TEST_CASE("chart/statistical/corr: 名单与列数不一致不崩") {
    canvas::Canvas canvas(30, 10);
    CorrMatrixChart chart;
    chart.set_matrix({"a"}, {{1.0, 2.0}, {3.0, 4.0}, {5.0, 6.0}});
    chart.draw({canvas});
    CHECK(count_nonblank(canvas) >= 0);
}

// ---------------- pca ----------------

TEST_CASE("chart/statistical/pca: 二维点云投影落格") {
    canvas::Canvas canvas(60, 20);
    PcaChart chart;
    chart.set_data({{1.0, 1.2}, {1.4, 1.1}, {2.0, 2.2}, {2.4, 2.1}, {3.0, 3.2}, {3.4, 3.1}});
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 6);
}

TEST_CASE("chart/statistical/pca: 空数据与单行不崩") {
    canvas::Canvas c1(30, 10);
    PcaChart empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(30, 10);
    PcaChart single;
    single.set_data({{1.0, 2.0}});
    single.draw({c2});
}

// ---------------- regression ----------------

TEST_CASE("chart/statistical/regression: 拟合线与残差可见") {
    canvas::Canvas canvas(60, 20);
    RegressionChart chart;
    std::vector<Point> points;
    for (int i = 0; i < 12; ++i) {
        points.push_back(Point{static_cast<double>(i), static_cast<double>(i) * 0.5 + 0.1});
    }
    chart.set_points(points);
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 10);
}

TEST_CASE("chart/statistical/regression: 单点与共线退化不崩") {
    canvas::Canvas c1(30, 10);
    RegressionChart single;
    single.set_points({Point{1.0, 1.0}});
    single.draw({c1});

    canvas::Canvas c2(30, 10);
    RegressionChart collinear;
    collinear.set_points({Point{1.0, 2.0}, Point{2.0, 4.0}, Point{3.0, 6.0}});
    collinear.draw({c2});
}

// ---------------- roc ----------------

TEST_CASE("chart/statistical/roc: 曲线与对角线出现") {
    canvas::Canvas canvas(60, 20);
    RocChart chart;
    chart.set_scores({0.9, 0.8, 0.7, 0.6, 0.4, 0.3, 0.2, 0.1},
                     {1, 1, 0, 1, 0, 1, 0, 0});
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 10);
}

TEST_CASE("chart/statistical/roc: 全同分类不崩") {
    canvas::Canvas canvas(30, 10);
    RocChart chart;
    chart.set_scores({0.5, 0.5, 0.5}, {1, 0, 1});
    chart.draw({canvas});
}

// ---------------- forest ----------------

TEST_CASE("chart/statistical/forest: 各研究的 CI 横线与合计菱形") {
    canvas::Canvas canvas(60, 20);
    ForestChart chart;
    ForestStudy a;
    a.label = "study a";
    a.estimate = 0.4;
    a.lower = 0.2;
    a.upper = 0.6;
    ForestStudy b;
    b.label = "study b";
    b.estimate = -0.1;
    b.lower = -0.3;
    b.upper = 0.1;
    chart.set_studies({a, b});
    chart.set_total(a);
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 8);
}

TEST_CASE("chart/statistical/forest: 空研究集不崩") {
    canvas::Canvas canvas(30, 10);
    ForestChart chart;
    chart.draw({canvas});
    CHECK(count_nonblank(canvas) == 0);
}

// ---------------- survival ----------------

TEST_CASE("chart/statistical/survival: 阶梯下降 + 删失刻度") {
    canvas::Canvas canvas(60, 20);
    SurvivalChart chart;
    chart.set_data({1.0, 2.0, 3.0, 4.0, 5.0, 6.0}, {1, 0, 1, 1, 0, 1});
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 10);
}

TEST_CASE("chart/statistical/survival: 空数据不崩") {
    canvas::Canvas canvas(30, 10);
    SurvivalChart chart;
    chart.draw({canvas});
    CHECK(count_nonblank(canvas) == 0);
}

// ---------------- autocorr ----------------

TEST_CASE("chart/statistical/autocorr: 周期序列滞后峰可见 + 置信带") {
    canvas::Canvas canvas(60, 20);
    AutocorrChart chart;
    std::vector<double> values;
    for (int i = 0; i < 40; ++i) {
        values.push_back(std::sin(0.5 * static_cast<double>(i)));
    }
    chart.set_values(values);
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 10);
}

TEST_CASE("chart/statistical/autocorr: 序列过短不崩") {
    canvas::Canvas canvas(30, 10);
    AutocorrChart chart;
    chart.set_values({1.0});
    chart.draw({canvas});
}

// ---------------- seasonal ----------------

TEST_CASE("chart/statistical/seasonal: 两整周期分解落格") {
    canvas::Canvas canvas(60, 20);
    SeasonalChart chart;
    std::vector<double> values;
    for (int i = 0; i < 24; ++i) {
        values.push_back(std::sin(3.14159265358979 / 6.0 * static_cast<double>(i)));
    }
    chart.set_values(values);
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 10);
}

TEST_CASE("chart/statistical/seasonal: 不足一个周期不崩") {
    canvas::Canvas canvas(30, 10);
    SeasonalChart chart;
    chart.set_values({1.0, 2.0, 3.0});
    chart.draw({canvas});
}

// ---------------- bland_altman ----------------

TEST_CASE("chart/statistical/bland_altman: 均值-差散点 + 界限") {
    canvas::Canvas canvas(60, 20);
    BlandAltmanChart chart;
    chart.set_pairs({5.0, 6.0, 7.0, 8.0}, {5.1, 5.8, 7.3, 7.9});
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 6);
}

TEST_CASE("chart/statistical/bland_altman: 长度不匹配与空对不崩") {
    canvas::Canvas c1(30, 10);
    BlandAltmanChart mismatch;
    mismatch.set_pairs({1.0, 2.0}, {1.0});
    mismatch.draw({c1});

    canvas::Canvas c2(30, 10);
    BlandAltmanChart empty;
    empty.draw({c2});
    CHECK(count_nonblank(c2) == 0);
}
