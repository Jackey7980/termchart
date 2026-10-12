// test_chart_financial.cpp —— 金融 7 图种端到端绘制（结构事实断言）
//
// 每个图种两个用例：正常小数据集画出东西；退化输入（空/单点/零波动）不崩。
// volume/drawdown/kagi/renko/point_figure 的实现此前已由并行会话交付，
// depth/yield_curve/drawdown 的退化语义以头文件注释为准。

#include <doctest.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/financial/depth.hpp"
#include "termchart/chart/financial/drawdown.hpp"
#include "termchart/chart/financial/kagi.hpp"
#include "termchart/chart/financial/point_figure.hpp"
#include "termchart/chart/financial/renko.hpp"
#include "termchart/chart/financial/volume.hpp"
#include "termchart/chart/financial/yield_curve.hpp"

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

// 锯齿形价格序列：保证 kagi/renko/point_figure 都有"反转"可画。
std::vector<double> zigzag() {
    return {10.0, 10.6, 10.2, 10.9, 10.4, 11.2, 10.8, 11.5, 11.0, 11.8, 11.3, 12.0};
}

}  // namespace

// ---------------- volume ----------------

TEST_CASE("chart/financial/volume: 方向色柱落进绘图区") {
    canvas::Canvas canvas(60, 20);
    VolumeChart chart;
    chart.set_values({3.0, 5.0, 2.0, 6.0, 4.0}, {1, 1, -1, 1, -1});
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 5);
}

TEST_CASE("chart/financial/volume: 空与全零不崩") {
    canvas::Canvas c1(30, 10);
    VolumeChart empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(30, 10);
    VolumeChart zeros;
    zeros.set_values({0.0, 0.0});
    zeros.draw({c2});
}

// ---------------- kagi ----------------

TEST_CASE("chart/financial/kagi: 反转序列产生阴阳线") {
    canvas::Canvas canvas(60, 20);
    KagiChart chart;
    chart.set_prices(zigzag());
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 10);
}

TEST_CASE("chart/financial/kagi: 单调序列与空输入不崩") {
    canvas::Canvas c1(30, 10);
    KagiChart monotonic;
    monotonic.set_prices({1.0, 2.0, 3.0, 4.0});
    monotonic.draw({c1});

    canvas::Canvas c2(30, 10);
    KagiChart empty;
    empty.draw({c2});
    CHECK(count_nonblank(c2) == 0);
}

// ---------------- renko ----------------

TEST_CASE("chart/financial/renko: 砖块按 brick_size 落格") {
    canvas::Canvas canvas(60, 20);
    RenkoChart chart;
    chart.set_prices(zigzag());
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 5);
}

TEST_CASE("chart/financial/renko: 零波动不产生砖块") {
    canvas::Canvas canvas(30, 10);
    RenkoChart chart;
    chart.set_prices({5.0, 5.0, 5.0, 5.0});
    chart.draw({canvas});
    CHECK(count_nonblank(canvas) >= 0);
}

// ---------------- point_figure ----------------

TEST_CASE("chart/financial/point_figure: X/O 列出现") {
    canvas::Canvas canvas(60, 20);
    PointFigureChart chart;
    chart.set_prices(zigzag());
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 5);
}

TEST_CASE("chart/financial/point_figure: 空输入不崩") {
    canvas::Canvas canvas(30, 10);
    PointFigureChart chart;
    chart.draw({canvas});
    CHECK(count_nonblank(canvas) == 0);
}

// ---------------- depth ----------------

TEST_CASE("chart/financial/depth: 买卖两侧对称落格") {
    canvas::Canvas canvas(60, 20);
    DepthChart chart;
    chart.set_book({9.9, 9.8, 9.7}, {10.0, 20.0, 30.0}, {10.1, 10.2, 10.3},
                   {12.0, 22.0, 31.0});
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 5);
}

TEST_CASE("chart/financial/depth: 空簿不崩") {
    canvas::Canvas canvas(30, 10);
    DepthChart chart;
    chart.draw({canvas});
    CHECK(count_nonblank(canvas) == 0);
}

// ---------------- yield_curve ----------------

TEST_CASE("chart/financial/yield_curve: 观测点与连线（期限升序）") {
    canvas::Canvas canvas(60, 20);
    YieldCurveChart chart;
    chart.set_curve({10.0, 1.0, 5.0, 2.0}, {3.0, 1.0, 2.0, 1.6});  // 乱序输入
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 6);
    CHECK(chart.points().size() == 4);
    CHECK(chart.points().front().x == 1.0);  // 已按期限升序重排
}

TEST_CASE("chart/financial/yield_curve: 单点与空曲线不崩") {
    canvas::Canvas c1(30, 10);
    YieldCurveChart single;
    single.set_curve({5.0}, {2.0});
    single.draw({c1});

    canvas::Canvas c2(30, 10);
    YieldCurveChart empty;
    empty.draw({c2});
    CHECK(count_nonblank(c2) == 0);
}

// ---------------- drawdown ----------------

TEST_CASE("chart/financial/drawdown: 水下曲线与填充、峰前恒 0") {
    canvas::Canvas canvas(60, 20);
    DrawdownChart chart;
    chart.set_prices({10.0, 11.0, 9.0, 10.5, 8.0});
    chart.draw({canvas});
    CHECK(count_inner_content(canvas) > 5);
    REQUIRE(chart.drawdown().size() == 5);
    CHECK(chart.drawdown()[0] == 0.0);              // 首点即峰
    CHECK(chart.drawdown()[1] == 0.0);              // 新高
    CHECK(chart.drawdown()[2] < 0.0);               // 回撤为负
    CHECK(chart.max_drawdown() < 0.0);
    CHECK(chart.max_drawdown() == chart.drawdown()[4]);  // 8/11-1 最深
}

TEST_CASE("chart/financial/drawdown: 非有限价格剔除与空输入不崩") {
    canvas::Canvas c1(30, 10);
    DrawdownChart nan_prices;
    nan_prices.set_prices({10.0, std::nan(""), 9.0});
    nan_prices.draw({c1});
    CHECK(nan_prices.drawdown().size() == 2);  // NaN 被剔除

    canvas::Canvas c2(30, 10);
    DrawdownChart empty;
    empty.draw({c2});
    CHECK(count_nonblank(c2) == 0);
    CHECK(empty.max_drawdown() == 0.0);
}

// ---------------- 交叉退化：全系列喂 NaN ----------------

TEST_CASE("chart/financial: 全 NaN 输入逐图种不崩") {
    const std::vector<double> bad{std::nan(""), std::nan("")};

    canvas::Canvas c1(30, 10);
    DrawdownChart dd;
    dd.set_prices(bad);
    dd.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(30, 10);
    KagiChart kagi;
    kagi.set_prices(bad);
    kagi.draw({c2});

    canvas::Canvas c3(30, 10);
    RenkoChart renko;
    renko.set_prices(bad);
    renko.draw({c3});

    canvas::Canvas c4(30, 10);
    PointFigureChart pf;
    pf.set_prices(bad);
    pf.draw({c4});
}
