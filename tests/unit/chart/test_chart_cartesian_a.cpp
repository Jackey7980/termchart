// test_chart_cartesian_a.cpp —— 气泡 / K 线 / 箱线 / 小提琴 / 等高线 / 误差棒 / 瀑布 / 甘特
//
// 只断言结构事实（出现了什么字符、落在哪个区域、退化输入不崩），
// 不做逐字节 golden。
#include <doctest.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/boxplot.hpp"
#include "termchart/chart/cartesian/bubble.hpp"
#include "termchart/chart/cartesian/candlestick.hpp"
#include "termchart/chart/cartesian/contour.hpp"
#include "termchart/chart/cartesian/errorbar.hpp"
#include "termchart/chart/cartesian/gantt.hpp"
#include "termchart/chart/cartesian/violin.hpp"
#include "termchart/chart/cartesian/waterfall.hpp"
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

bool has_char(const canvas::Canvas& c, char32_t cp) {
    for (std::uint32_t y = 0; y < c.height(); ++y) {
        for (std::uint32_t x = 0; x < c.width(); ++x) {
            if (c.at(x, y).cp == cp) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

// ---------- bubble ----------

TEST_CASE("chart/bubble: 半径映射为大小阶梯且产生可见内容") {
    canvas::Canvas canvas(50, 16);
    BubbleChart chart;
    chart.set_data({{Point{0.0, 0.0}, 0.1},
                    {Point{5.0, 5.0}, 1.0},
                    {Point{10.0, 0.0}, 5.0}});
    chart.draw({canvas});

    CHECK(count_nonblank(canvas) > 5);
    CHECK(has_char(canvas, U'\u25CF'));   // 最大档 ●
    CHECK(has_char(canvas, U'\u00B7'));   // 最小档 ·
    CHECK(chart.last_area().inner_rect().w > 0.0);
}

TEST_CASE("chart/bubble: 空数据与非有限位置不绘制不崩") {
    canvas::Canvas c1(30, 10);
    BubbleChart empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(30, 10);
    BubbleChart nan_pos;
    nan_pos.set_data({{Point{std::nan(""), 1.0}, 2.0},
                      {Point{1.0, std::nan("")}, 2.0},
                      {Point{1.0, 1.0}, std::nan("")}});  // r 非有限按 0 仍画
    nan_pos.draw({c2});
    CHECK(count_nonblank(c2) > 0);
    CHECK_FALSE(has_char(c2, U'\u25CF'));  // NaN r 归 0，不进最大档
}

// ---------- candlestick ----------

TEST_CASE("chart/candlestick: 实体 █ 与影线 │ 都出现") {
    canvas::Canvas canvas(50, 16);
    CandlestickChart chart;
    chart.set_data({{10.0, 12.0, 9.0, 11.0},   // 涨
                    {11.0, 11.5, 8.0, 8.5},    // 跌
                    {9.0, 14.0, 8.5, 13.0}});  // 长影线
    chart.draw({canvas});

    CHECK(count_nonblank(canvas) > 10);
    CHECK(has_char(canvas, U'\u2588'));
    CHECK(has_char(canvas, U'\u2502'));
}

TEST_CASE("chart/candlestick: 十字星退化为单行 ─；空数据不崩") {
    canvas::Canvas canvas(40, 12);
    CandlestickChart chart;
    chart.set_data({{10.0, 11.0, 9.0, 10.0}});
    chart.draw({canvas});
    CHECK(has_char(canvas, U'\u2500'));
    CHECK_FALSE(has_char(canvas, U'\u2588'));

    canvas::Canvas c2(30, 10);
    CandlestickChart empty;
    empty.set_data({{std::nan(""), 0.0, 0.0, 0.0}});  // 非有限整根剔除
    empty.draw({c2});
    CHECK(count_nonblank(c2) == 0);
}

// ---------- boxplot ----------

TEST_CASE("chart/boxplot: 箱体/须/离群点齐全") {
    canvas::Canvas canvas(50, 16);
    BoxPlotChart chart;
    chart.set_data({{0.0, 2.0, 3.0, 4.0, 6.0, {8.0}},
                    {1.0, 3.0, 4.0, 5.0, 7.0, {}}});
    chart.draw({canvas});

    CHECK(count_nonblank(canvas) > 10);
    CHECK(has_char(canvas, U'\u2588'));  // 箱体
    CHECK(has_char(canvas, U'\u25CB'));  // 离群点
}

TEST_CASE("chart/boxplot: 五数违反序关系的组被剔除；空数据不崩") {
    canvas::Canvas c1(40, 12);
    BoxPlotChart bad;
    bad.set_data({{5.0, 2.0, 3.0, 4.0, 6.0, {}}});  // lo > q1，剔除
    bad.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(40, 12);
    BoxPlotChart empty;
    empty.draw({c2});
    CHECK(count_nonblank(c2) == 0);
}

// ---------- violin ----------

TEST_CASE("chart/violin: 密度条带可见且中位数有标记") {
    canvas::Canvas canvas(50, 18);
    ViolinChart chart;
    std::vector<double> a;
    for (int i = 0; i < 30; ++i) {
        a.push_back(1.0 + 0.1 * static_cast<double>(i % 10));
    }
    chart.set_data({a});
    chart.draw({canvas});

    CHECK(count_nonblank(canvas) > 10);
    CHECK(has_char(canvas, U'\u2588'));
    CHECK(has_char(canvas, U'\u2500'));  // 中位数标记
}

TEST_CASE("chart/violin: 空组/全同值样本不崩") {
    canvas::Canvas c1(40, 12);
    ViolinChart empty;
    empty.set_data({{}, {std::nan(""), std::nan("")}});
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(40, 12);
    ViolinChart same;
    same.set_data({{5.0, 5.0, 5.0, 5.0}});  // σ=0：带宽退化路径
    same.draw({c2});
    CHECK(count_nonblank(c2) > 0);
}

// ---------- contour ----------

TEST_CASE("chart/contour: 线性场提取出 · 等值线") {
    canvas::Canvas canvas(50, 18);
    ContourChart chart;
    // 线性场 v = x + y，等值线应是斜线，必然穿过格点。
    std::vector<std::vector<double>> field;
    for (int j = 0; j < 10; ++j) {
        std::vector<double> row;
        for (int i = 0; i < 10; ++i) {
            row.push_back(static_cast<double>(i + j));
        }
        field.push_back(row);
    }
    chart.set_field(field);
    chart.draw({canvas});

    CHECK(count_nonblank(canvas) > 5);
    CHECK(has_char(canvas, U'\u00B7'));
}

TEST_CASE("chart/contour: 场太小/无有效值不崩不画；显式级生效") {
    canvas::Canvas c1(30, 10);
    ContourChart tiny;
    tiny.set_field({{1.0, 2.0, 3.0}});  // 只有一行
    tiny.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(30, 10);
    ContourChart nan_field;
    nan_field.set_field({{std::nan(""), std::nan("")},
                         {std::nan(""), std::nan("")}});
    nan_field.draw({c2});
    CHECK(count_nonblank(c2) == 0);

    canvas::Canvas c3(40, 14);
    ContourChart custom;
    std::vector<std::vector<double>> field;
    for (int j = 0; j < 6; ++j) {
        std::vector<double> row;
        for (int i = 0; i < 6; ++i) {
            row.push_back(static_cast<double>(i * j));
        }
        field.push_back(row);
    }
    custom.set_field(field);
    custom.set_options({Color::ansi16(6), {2.5}, 4});
    custom.draw({c3});
    CHECK(has_char(c3, U'\u00B7'));
}

// ---------- errorbar ----------

TEST_CASE("chart/errorbar: 竖向误差线与中心点都出现") {
    canvas::Canvas canvas(50, 16);
    ErrorBarChart chart;
    chart.set_data({{1.0, 5.0, 3.0, 7.0, 0.0, 0.0},
                    {5.0, 8.0, 6.0, 10.0, 0.0, 0.0}});
    chart.draw({canvas});

    CHECK(count_nonblank(canvas) > 5);
    CHECK(has_char(canvas, U'\u25CF'));
    CHECK(has_char(canvas, U'\u2502'));
}

TEST_CASE("chart/errorbar: 空数据不崩；非有限误差按无误差处理") {
    canvas::Canvas c1(30, 10);
    ErrorBarChart empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(30, 10);
    ErrorBarChart loose;
    loose.set_data(
        {{1.0, 2.0, std::nan(""), std::nan(""), std::nan(""), std::nan("")}});
    loose.draw({c2});
    CHECK(count_nonblank(c2) > 0);
    CHECK(has_char(c2, U'\u25CF'));
}

// ---------- waterfall ----------

TEST_CASE("chart/waterfall: 柱可见且累计序列正确") {
    canvas::Canvas canvas(50, 16);
    WaterfallChart chart;
    chart.set_data({{"a", 10.0, false}, {"b", -4.0, false}, {"c", 5.0, false}});
    chart.draw({canvas});

    CHECK(count_nonblank(canvas) > 10);
    CHECK(has_char(canvas, U'\u2588'));
    REQUIRE(chart.cumulative().size() == 3);
    CHECK(chart.cumulative()[0] == doctest::Approx(10.0));
    CHECK(chart.cumulative()[1] == doctest::Approx(6.0));
    CHECK(chart.cumulative()[2] == doctest::Approx(11.0));
}

TEST_CASE("chart/waterfall: 小计项重置基线；空数据不绘制") {
    WaterfallChart chart;
    chart.set_data({{"a", 10.0, false}, {"t", 7.0, true}, {"b", 2.0, false}});
    REQUIRE(chart.cumulative().size() == 3);
    CHECK(chart.cumulative()[1] == doctest::Approx(7.0));   // 小计：重置
    CHECK(chart.cumulative()[2] == doctest::Approx(9.0));   // 从 7 起算

    canvas::Canvas c2(30, 10);
    WaterfallChart empty;
    empty.set_data({{"nan", std::nan(""), false}});
    empty.draw({c2});
    CHECK(count_nonblank(c2) == 0);
}

// ---------- gantt ----------

TEST_CASE("chart/gantt: 时间条与行标签都出现") {
    canvas::Canvas canvas(50, 12);
    GanttChart chart;
    chart.set_data({{"alpha", 0.0, 3.0}, {"beta", 2.0, 5.0}, {"gamma", 4.0, 6.0}});
    chart.draw({canvas});

    CHECK(count_nonblank(canvas) > 10);
    CHECK(has_char(canvas, U'\u2588'));
    CHECK(has_char(canvas, U'\u0067'));  // "gamma" 的 g（行标签）
}

TEST_CASE("chart/gantt: 空数据不崩；end<start 的任务被剔除") {
    canvas::Canvas c1(30, 10);
    GanttChart empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(30, 10);
    GanttChart bad;
    bad.set_data({{"bad", 5.0, 2.0}, {"good", 1.0, 3.0}});
    bad.draw({c2});
    CHECK(count_nonblank(c2) > 0);
    CHECK_FALSE(has_char(c2, U'\u0062'));  // "bad" 的 b 不应出现
}
