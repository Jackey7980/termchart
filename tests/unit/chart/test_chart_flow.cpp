// test_chart_flow.cpp —— funnel/stream 端到端绘制（结构事实断言，不做逐字节 golden）
#include <doctest.h>

#include <cstdint>
#include <limits>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/flow/funnel.hpp"
#include "termchart/chart/flow/stream.hpp"
#include "termchart/chart/series.hpp"
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

constexpr char32_t kBlock = U'\u2588';  // █ 条体

}  // namespace

// ---------------- funnel ----------------

TEST_CASE("chart/flow/funnel: 逐段横条递缩且首段满宽") {
    canvas::Canvas canvas(40, 14);
    FunnelChart chart;
    chart.set_title("funnel");
    chart.set_stages({{"visit", 100.0, Color::none()},
                      {"cart", 70.0, Color::none()},
                      {"pay", 40.0, Color::none()},
                      {"done", 20.0, Color::none()}});
    chart.draw({canvas});

    CHECK(count_char(canvas, kBlock) > 50);
    // 首段（值最大）从内容区左缘起满宽铺 █；行首标签文本会盖住 x<=8 的格。
    CHECK(canvas.at(10, 1).cp == kBlock);
}

TEST_CASE("chart/flow/funnel: 空阶段、全零与全负不绘制") {
    canvas::Canvas c1(40, 14);
    FunnelChart empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(40, 14);
    FunnelChart zeros;
    zeros.set_stages({{"a", 0.0, Color::none()}, {"b", 0.0, Color::none()}});
    zeros.draw({c2});
    CHECK(count_nonblank(c2) == 0);

    canvas::Canvas c3(40, 14);
    FunnelChart negative;
    negative.set_stages({{"a", -5.0, Color::none()}, {"b", -1.0, Color::none()}});
    negative.draw({c3});
    CHECK(count_nonblank(c3) == 0);  // vmax <= 0 → 整体放弃
}

TEST_CASE("chart/flow/funnel: 阶段数多于行数不崩且截断") {
    canvas::Canvas canvas(40, 8);  // 内容区仅 4 行
    FunnelChart chart;
    std::vector<FunnelStage> stages;
    for (int i = 0; i < 20; ++i) {
        stages.push_back(FunnelStage{"s", 10.0, Color::none()});
    }
    chart.set_stages(std::move(stages));
    REQUIRE_NOTHROW(chart.draw({canvas}));
    CHECK(count_char(canvas, kBlock) > 0);
}

// ---------------- stream ----------------

TEST_CASE("chart/flow/stream: 中心对称堆叠铺满且两系列都落格") {
    canvas::Canvas canvas(60, 20);
    StreamChart chart;
    chart.set_title("stream");
    std::vector<Series> series;
    Series s1;
    s1.name = "s1";
    for (int i = 0; i < 5; ++i) {
        s1.points.push_back(Point{static_cast<double>(i), 1.0 + static_cast<double>(i)});
    }
    Series s2;
    s2.name = "s2";
    for (int i = 0; i < 5; ++i) {
        s2.points.push_back(Point{static_cast<double>(i), 2.0});
    }
    series.push_back(std::move(s1));
    series.push_back(std::move(s2));
    chart.set_series(std::move(series));
    chart.draw({canvas});

    CHECK(count_char(canvas, kBlock) > 60);
}

TEST_CASE("chart/flow/stream: 底部堆叠模式照常绘制") {
    canvas::Canvas canvas(60, 20);
    StreamChart chart;
    chart.options.centered = false;
    std::vector<Series> series;
    Series s1;
    s1.name = "only";
    for (int i = 0; i < 4; ++i) {
        s1.points.push_back(Point{static_cast<double>(i), 3.0});
    }
    series.push_back(std::move(s1));
    chart.set_series(std::move(series));
    chart.draw({canvas});

    CHECK(count_char(canvas, kBlock) > 20);
}

TEST_CASE("chart/flow/stream: 空系列、空点与全零值不绘制") {
    canvas::Canvas c1(40, 12);
    StreamChart empty;
    empty.draw({c1});
    CHECK(count_nonblank(c1) == 0);

    canvas::Canvas c2(40, 12);
    StreamChart no_points;
    std::vector<Series> series(1);
    series[0].name = "s";
    no_points.set_series(std::move(series));
    no_points.draw({c2});
    CHECK(count_nonblank(c2) == 0);

    canvas::Canvas c3(40, 12);
    StreamChart zeros;
    std::vector<Series> zs(2);
    for (int i = 0; i < 3; ++i) {
        zs[0].points.push_back(Point{static_cast<double>(i), 0.0});
        zs[1].points.push_back(Point{static_cast<double>(i), 0.0});
    }
    zeros.set_series(std::move(zs));
    zeros.draw({c3});
    CHECK(count_nonblank(c3) == 0);
}

TEST_CASE("chart/flow/stream: 系列点数不齐不崩") {
    canvas::Canvas canvas(60, 20);
    StreamChart chart;
    std::vector<Series> series(2);
    series[0].name = "long";
    series[1].name = "short";
    for (int i = 0; i < 6; ++i) {
        series[0].points.push_back(Point{static_cast<double>(i), 2.0});
    }
    for (int i = 0; i < 2; ++i) {
        series[1].points.push_back(Point{static_cast<double>(i), 2.0});
    }
    chart.set_series(std::move(series));
    REQUIRE_NOTHROW(chart.draw({canvas}));
    CHECK(count_char(canvas, kBlock) > 0);
}
