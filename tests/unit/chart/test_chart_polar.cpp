// test_chart_polar.cpp —— 极坐标八图种端到端绘制（结构事实断言，不做逐字节 golden）
#include <doctest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/polar/donut.hpp"
#include "termchart/chart/polar/gauge.hpp"
#include "termchart/chart/polar/pie.hpp"
#include "termchart/chart/polar/polar_bar.hpp"
#include "termchart/chart/polar/polar_heat.hpp"
#include "termchart/chart/polar/polar_line.hpp"
#include "termchart/chart/polar/radar.hpp"
#include "termchart/chart/polar/rose.hpp"
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

std::size_t count_char(const canvas::Canvas& c, char32_t cp) {
    std::size_t n = 0;
    for (std::uint32_t y = 0; y < c.height(); ++y) {
        for (std::uint32_t x = 0; x < c.width(); ++x) {
            if (c.at(x, y).cp == cp) {
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

constexpr char32_t kBlock = U'\u2588';      // █
constexpr char32_t kDark = U'\u2593';       // ▓
constexpr char32_t kMedium = U'\u2592';     // ▒
constexpr char32_t kLight = U'\u2591';      // ░
constexpr char32_t kDot = U'\u00B7';        // · 参考环/蛛网
constexpr char32_t kLine = U'\u2022';       // • 数据线
constexpr char32_t kMarker = U'\u25CF';     // ● 数据点/弧体/轴帽
constexpr char32_t kNeedle = U'*';          // 指针
constexpr char32_t kTick = U'+';            // 仪表刻度

std::size_t flag(bool present) { return present ? std::size_t{1} : std::size_t{0}; }

}  // namespace

// ---------------- pie ----------------

TEST_CASE("chart/polar/pie: 四扇区落进绘图区且扇区字符可分") {
    canvas::Canvas canvas(60, 20);
    PieChart chart;
    chart.set_title("pie");
    chart.set_values({4.0, 3.0, 2.0, 1.0}, {"a", "b", "c", "d"});
    chart.draw({canvas});

    CHECK(count_inner_content(canvas) > 50);
    // 四个扇区各持专属字符：至少 3 种同时出现（最小扇区占 1/10 盘面，必落格）。
    const std::size_t distinct = flag(count_char(canvas, kBlock) > 0) +
                                 flag(count_char(canvas, kDark) > 0) +
                                 flag(count_char(canvas, kMedium) > 0) +
                                 flag(count_char(canvas, kLight) > 0);
    CHECK(distinct >= 3);
}

TEST_CASE("chart/polar/pie: 空数据、全零与无效值不崩") {
    canvas::Canvas c1(20, 10);
    PieChart empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(20, 10);
    PieChart zeros;
    zeros.set_values({0.0, 0.0, 0.0});
    zeros.draw({c2});
    CHECK(count_nonblank(c2) == 0);

    // 负值与 NaN 被滤除，其余照画。
    canvas::Canvas c3(60, 20);
    PieChart partial;
    partial.set_values({-5.0, std::numeric_limits<double>::quiet_NaN(), 10.0});
    partial.draw({c3});
    CHECK(count_inner_content(c3) > 50);
}

// ---------------- donut ----------------

TEST_CASE("chart/polar/donut: 环带填充且中心孔留空") {
    canvas::Canvas canvas(60, 20);
    DonutChart chart;
    chart.set_values({5.0, 5.0}, {"x", "y"});
    chart.draw({canvas});

    CHECK(count_inner_content(canvas) > 50);
    // 内框几何中心在孔内：必须是空格。
    CHECK(canvas.at(29, 9).cp == U'\u0020');
    CHECK(canvas.at(29, 9).cp != kBlock);
}

TEST_CASE("chart/polar/donut: 空数据不绘制") {
    canvas::Canvas canvas(20, 10);
    DonutChart chart;
    chart.draw({canvas});
    CHECK(count_nonblank(canvas) == 0);
}

// ---------------- rose ----------------

TEST_CASE("chart/polar/rose: 等角扇区按值定径且外圈参考环存在") {
    canvas::Canvas canvas(60, 20);
    RoseChart chart;
    chart.set_values({1.0, 2.0, 3.0, 4.0}, {"q1", "q2", "q3", "q4"});
    chart.draw({canvas});

    CHECK(count_inner_content(canvas) > 30);
    CHECK(count_char(canvas, kDot) > 0);  // 外圈参考环
    const std::size_t distinct = flag(count_char(canvas, kBlock) > 0) +
                                 flag(count_char(canvas, kDark) > 0) +
                                 flag(count_char(canvas, kMedium) > 0) +
                                 flag(count_char(canvas, kLight) > 0);
    CHECK(distinct >= 3);
}

TEST_CASE("chart/polar/rose: 空数据不绘制") {
    canvas::Canvas c1(20, 10);
    RoseChart empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(20, 10);
    RoseChart zeros;
    zeros.set_values({0.0, 0.0});
    zeros.draw({c2});
    CHECK(count_nonblank(c2) == 0);
}

// ---------------- radar ----------------

TEST_CASE("chart/polar/radar: 蛛网、数据多边形与顶点三字符齐备") {
    canvas::Canvas canvas(60, 20);
    RadarChart chart;
    chart.set_title("radar");
    chart.set_values({3.0, 4.0, 5.0, 2.0, 4.5}, {"a", "b", "c", "d", "e"});
    chart.draw({canvas});

    CHECK(count_char(canvas, kDot) > 0);    // 蛛网（环 + 射线）
    CHECK(count_char(canvas, kLine) > 0);   // 数据多边形描边
    CHECK(count_char(canvas, kMarker) > 0); // 数据顶点
    CHECK(count_inner_content(canvas) > 30);
}

TEST_CASE("chart/polar/radar: 少于 3 根轴不绘制") {
    canvas::Canvas canvas(20, 10);
    RadarChart chart;
    chart.set_values({1.0, 2.0});
    chart.draw({canvas});
    CHECK(count_nonblank(canvas) == 0);
}

TEST_CASE("chart/polar/radar: 全零数据只画蛛网不崩") {
    canvas::Canvas canvas(60, 20);
    RadarChart chart;
    chart.set_values({0.0, 0.0, 0.0, 0.0, 0.0});
    chart.draw({canvas});
    CHECK(count_char(canvas, kDot) > 0);    // 蛛网照画
    CHECK(count_char(canvas, kMarker) >= 1);  // 数据缩在中心
}

// ---------------- gauge ----------------

TEST_CASE("chart/polar/gauge: 弧体、刻度与指针齐备") {
    canvas::Canvas canvas(60, 20);
    GaugeChart chart;
    chart.set_title("cpu");
    chart.set_value(50.0);
    chart.draw({canvas});

    CHECK(count_char(canvas, kMarker) > 0);  // 弧体 + 轴帽
    CHECK(count_char(canvas, kTick) >= 9);   // 主刻度（弧体 + 内侧各一粒）
    CHECK(count_char(canvas, kNeedle) > 0);  // 指针
}

TEST_CASE("chart/polar/gauge: 值域退化不绘制") {
    canvas::Canvas canvas(20, 10);
    GaugeChart chart;
    GaugeOptions opts;
    opts.value_min = 10.0;
    opts.value_max = 10.0;
    chart.set_options(opts);
    chart.set_value(10.0);
    chart.draw({canvas});
    CHECK(count_nonblank(canvas) == 0);
}

TEST_CASE("chart/polar/gauge: 非有限值不画指针但画盘面") {
    canvas::Canvas canvas(60, 20);
    GaugeChart chart;
    chart.set_value(std::numeric_limits<double>::quiet_NaN());
    chart.draw({canvas});
    CHECK(count_char(canvas, kMarker) > 0);
    CHECK(count_char(canvas, kNeedle) == 0);
}

// ---------------- polar_bar ----------------

TEST_CASE("chart/polar/polar_bar: 径向柱落格且参考网格存在") {
    canvas::Canvas canvas(60, 20);
    PolarBarChart chart;
    chart.set_title("polar bar");
    chart.set_values({3.0, 6.0, 9.0}, {"n", "e", "s"});
    chart.draw({canvas});

    CHECK(count_char(canvas, kBlock) > 0);  // 柱体
    CHECK(count_char(canvas, kDot) > 0);    // 参考环 + 槽位射线
    CHECK(count_inner_content(canvas) > 30);
}

TEST_CASE("chart/polar/polar_bar: 空数据不绘制") {
    canvas::Canvas c1(20, 10);
    PolarBarChart empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(20, 10);
    PolarBarChart zeros;
    zeros.set_values({0.0, 0.0, 0.0});
    zeros.draw({c2});
    CHECK(count_nonblank(c2) == 0);
}

// ---------------- polar_line ----------------

TEST_CASE("chart/polar/polar_line: 数据点连线成环且外圈参考环存在") {
    canvas::Canvas canvas(60, 20);
    PolarLineChart chart;
    chart.set_title("polar line");
    std::vector<Point> pts;
    for (int i = 0; i < 12; ++i) {
        pts.push_back(Point{static_cast<double>(i) * 30.0, 6.0});
    }
    chart.set_points(pts);
    PolarLineOptions opts;
    opts.close = true;
    chart.set_options(opts);
    chart.draw({canvas});

    CHECK(count_char(canvas, kLine) > 0);    // 连线
    CHECK(count_char(canvas, kMarker) >= 10);  // 12 个顶点（相邻角在格上必分离）
    CHECK(count_char(canvas, kDot) > 0);     // 外圈参考环
}

TEST_CASE("chart/polar/polar_line: 空数据与单点不崩") {
    canvas::Canvas c1(20, 10);
    PolarLineChart empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(60, 20);
    PolarLineChart single;
    single.set_points({Point{45.0, 5.0}});
    single.draw({c2});
    CHECK(count_char(c2, kMarker) == 1);
}

// ---------------- polar_heat ----------------

TEST_CASE("chart/polar/polar_heat: 分箱上色且密度字符非空格") {
    canvas::Canvas canvas(60, 20);
    PolarHeatChart chart;
    chart.set_title("polar heat");
    std::vector<Point> pts;
    // 密集簇（角 0-90、半径 0-2）+ 稀疏散点，保证多档密度同时出现。
    for (int i = 0; i < 40; ++i) {
        pts.push_back(Point{static_cast<double>(i % 90), static_cast<double>(i % 3)});
    }
    pts.push_back(Point{180.0, 6.0});
    pts.push_back(Point{270.0, 8.0});
    chart.set_points(pts);
    chart.draw({canvas});

    CHECK(count_inner_content(canvas) > 10);
    const std::size_t distinct = flag(count_char(canvas, kBlock) > 0) +
                                 flag(count_char(canvas, kDark) > 0) +
                                 flag(count_char(canvas, kMedium) > 0) +
                                 flag(count_char(canvas, kLight) > 0);
    CHECK(distinct >= 2);
}

TEST_CASE("chart/polar/polar_heat: 空数据不绘制") {
    canvas::Canvas canvas(20, 10);
    PolarHeatChart chart;
    chart.draw({canvas});
    CHECK(count_nonblank(canvas) == 0);
}
