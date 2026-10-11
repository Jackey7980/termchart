// test_chart_extras.cpp —— 绘制上下文开关、数据集、散点图、迷你折线
#include <doctest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/chart/cartesian/scatter_chart.hpp"
#include "termchart/chart/context.hpp"
#include "termchart/chart/dataset.hpp"
#include "termchart/chart/sparkline.hpp"

namespace {

using namespace termchart;
using namespace termchart::chart;

bool has_char(const canvas::Canvas& canvas, char32_t cp) {
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            if (canvas.at(x, y).cp == cp) {
                return true;
            }
        }
    }
    return false;
}

std::size_t count_nonblank(const canvas::Canvas& canvas) {
    std::size_t count = 0;
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            if (canvas.at(x, y).cp != U' ') {
                ++count;
            }
        }
    }
    return count;
}

Series ramp_series() {
    Series series;
    series.name = "s";
    series.points = {{0.0, 0.0}, {1.0, 1.0}, {2.0, 0.5}, {3.0, 1.5}};
    return series;
}

}  // namespace

TEST_CASE("chart/context: 关掉轴/网格/标题/图例后只剩数据") {
    canvas::Canvas with_all(50, 14);
    LineChart chart;
    chart.set_title("T");
    chart.add_series(ramp_series());
    chart.draw(DrawContext{with_all});
    const std::size_t full = count_nonblank(with_all);
    CHECK(has_char(with_all, U'T'));  // 标题
    CHECK(has_char(with_all, U'x'));  // x 轴名
    CHECK(has_char(with_all, U'y'));  // y 轴名

    canvas::Canvas data_only(50, 14);
    DrawContext ctx{data_only};
    ctx.draw_axes = false;
    ctx.draw_grid = false;
    ctx.draw_legend = false;
    ctx.draw_title = false;
    chart.draw(ctx);

    CHECK_FALSE(has_char(data_only, U'T'));
    CHECK_FALSE(has_char(data_only, U'x'));
    CHECK_FALSE(has_char(data_only, U'y'));
    CHECK(count_nonblank(data_only) > 0);          // 数据还在
    CHECK(count_nonblank(data_only) < full);       // 但确实少了不少东西
    // 折线与数据点仍按原样画出
    CHECK(has_char(data_only, kMarkerChar));
}

TEST_CASE("chart/context: 网格样式可覆盖，未给时用默认暗灰") {
    canvas::Canvas canvas(40, 12);
    LineChart chart;
    chart.add_series(ramp_series());

    DrawContext ctx{canvas};
    ctx.draw_axes = false;
    ctx.draw_legend = false;
    ctx.draw_title = false;
    ctx.grid_style = Style{}.set_fg(Color::ansi16(13));
    chart.draw(ctx);

    bool found_grid = false;
    for (std::uint32_t y = 0; y < canvas.height() && !found_grid; ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            if (canvas.at(x, y).style.fg() == Color::ansi16(13)) {
                found_grid = true;
                break;
            }
        }
    }
    CHECK(found_grid);
}

TEST_CASE("chart/dataset: 范围查询、含零、忽略非有限值") {
    Dataset dataset;
    CHECK(dataset.empty());
    CHECK_FALSE(dataset.bounds().valid);
    CHECK(dataset.point_count() == 0);

    dataset.add("a", {1.0, -3.0, 2.0});
    CHECK(dataset.size() == 1);
    CHECK(dataset.point_count() == 3);
    const DataRange range = dataset.bounds();
    REQUIRE(range.valid);
    CHECK(range.ymin == doctest::Approx(-3.0));
    CHECK(range.ymax == doctest::Approx(2.0));
    CHECK(range.xmin == doctest::Approx(0.0));
    CHECK(range.xmax == doctest::Approx(2.0));

    Dataset positive;
    positive.add("b", {1.0, 4.0});
    CHECK(positive.bounds().ymin == doctest::Approx(1.0));
    CHECK(positive.bounds(true).ymin == doctest::Approx(0.0));  // 柱状图需要含零

    Dataset weird;
    weird.add("c", {std::numeric_limits<double>::quiet_NaN(), 5.0});
    const DataRange filtered = weird.bounds();
    REQUIRE(filtered.valid);
    CHECK(filtered.ymin == doctest::Approx(5.0));
    CHECK(filtered.ymax == doctest::Approx(5.0));

    // x/y 等长截断
    Dataset paired;
    paired.add("d", {1.0, 2.0, 3.0}, {10.0, 20.0});
    CHECK(paired.point_count() == 2);
    CHECK(paired.series()[0].points[1].y == doctest::Approx(20.0));

    dataset.clear();
    CHECK(dataset.empty());
}

TEST_CASE("chart/scatter: 气泡标记按权重四档归一化") {
    canvas::Canvas canvas(60, 18);
    ScatterChart chart;
    Series series;
    series.name = "bubble";
    series.points = {{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}, {3.0, 0.0}};
    chart.add_bubble_series(series, {1.0, 50.0, 100.0, 0.0});  // 比值 0.01 / 0.5 / 1.0 / 0
    chart.draw(DrawContext{canvas});

    CHECK(has_char(canvas, U'\u00B7'));  // 最小档
    CHECK(has_char(canvas, U'\u25CF'));  // 次大档
    CHECK(has_char(canvas, U'\u25C9'));  // 最大档

    // 非气泡系列用统一标记
    canvas::Canvas plain(60, 18);
    ScatterChart circles;
    circles.add_series(ramp_series());
    circles.draw(DrawContext{plain});
    CHECK(has_char(plain, kMarkerChar));
}

TEST_CASE("chart/scatter: 空数据不崩，轴与网格照常") {
    canvas::Canvas canvas(30, 10);
    ScatterChart chart;
    chart.draw(DrawContext{canvas});  // 没有任何系列
    CHECK(has_char(canvas, U'x'));
    CHECK(has_char(canvas, U'y'));
}

TEST_CASE("chart/sparkline: 铺满画布、末点带标记、数据不足不画") {
    canvas::Canvas canvas(10, 3);
    Sparkline spark;
    spark.set_values({0.0, 1.0, 0.5, 1.5, 1.0});
    const std::size_t written = spark.draw(canvas);

    CHECK(written > 0);
    CHECK(canvas.at(9, 0).cp == kMarkerChar);  // 末点落在最后一列（且标记覆盖线段）
    CHECK(spark.size() == 5);

    Sparkline tiny;
    tiny.set_values({1.0});
    canvas::Canvas other(10, 3);
    CHECK(tiny.draw(other) == 0);  // 单点画不出线

    Sparkline blank;
    CHECK(blank.draw(other) == 0);
    CHECK(other.at(0, 1).cp == U' ');  // 什么都没写
}

TEST_CASE("chart/sparkline: zero_baseline 影响纵向映射，NaN 不算域") {
    canvas::Canvas adaptive(5, 5);
    Sparkline plain;
    plain.set_values({10.0, 20.0});
    plain.set_marker(false);
    CHECK(plain.draw(adaptive) > 0);

    // 含零后，同样的数据会被压到上半部分；末点列在最低行（值 20 是最大 → 顶部）
    canvas::Canvas baseline(5, 5);
    Sparkline with_zero;
    with_zero.set_values({10.0, 20.0});
    with_zero.set_marker(false);
    with_zero.set_zero_baseline(true);
    CHECK(with_zero.draw(baseline) > 0);

    // 自适应时最小值贴底；含零后最小值不再贴底
    bool adaptive_bottom = false;
    for (std::uint32_t x = 0; x < adaptive.width(); ++x) {
        if (adaptive.at(x, 4).cp != U' ') {
            adaptive_bottom = true;
            break;
        }
    }
    bool baseline_bottom = false;
    for (std::uint32_t x = 0; x < baseline.width(); ++x) {
        if (baseline.at(x, 4).cp != U' ') {
            baseline_bottom = true;
            break;
        }
    }
    CHECK(adaptive_bottom);
    CHECK_FALSE(baseline_bottom);

    canvas::Canvas nan_canvas(6, 3);
    Sparkline nan_line;
    nan_line.set_values({std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::quiet_NaN()});
    CHECK(nan_line.draw(nan_canvas) == 0);  // 全是非有限值：什么都不画
}
