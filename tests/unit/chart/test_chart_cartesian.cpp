// test_chart_cartesian.cpp —— 折线图 / 柱状图 端到端绘制
#include <doctest.h>

#include <cmath>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/bar_chart.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/core/style.hpp"
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

}  // namespace

TEST_CASE("chart/line: 绘制后产生可见内容且内框正确") {
    canvas::Canvas canvas(60, 20);
    LineChart chart;
    chart.set_title("t");

    Series s;
    s.name = "sin";
    s.color = Color::ansi16(2);
    for (int i = 0; i <= 40; ++i) {
        const double x = static_cast<double>(i) / 4.0;
        s.points.push_back(Point{x, std::sin(x)});
    }
    chart.add_series(std::move(s));
    chart.draw({canvas});

    CHECK(count_nonblank(canvas) > 10);
    const layout::PlotArea& area = chart.last_area();
    CHECK(area.inner_rect().x == doctest::Approx(8.0));
    CHECK(area.inner_rect().bottom() == doctest::Approx(17.0));  // 20-1-3
    // 数据点标记 ● 应至少出现一次
    bool has_marker = false;
    for (std::uint32_t y = 0; y < 20 && !has_marker; ++y) {
        for (std::uint32_t x = 0; x < 60; ++x) {
            if (canvas.at(x, y).cp == U'\u25CF') {
                has_marker = true;
                break;
            }
        }
    }
    CHECK(has_marker);
}

TEST_CASE("chart/bar: 绘制后产生实心柱 █") {
    canvas::Canvas canvas(60, 20);
    BarChart chart;
    chart.set_values({4.0, 9.0, 6.0, 12.0}, {"a", "b", "c", "d"}, Color::ansi16(4),
                     "sales");
    chart.draw({canvas});

    CHECK(count_nonblank(canvas) > 10);
    bool has_bar = false;
    for (std::uint32_t y = 0; y < 20 && !has_bar; ++y) {
        for (std::uint32_t x = 0; x < 60; ++x) {
            if (canvas.at(x, y).cp == U'\u2588') {
                has_bar = true;
                break;
            }
        }
    }
    CHECK(has_bar);
}
