// test_chart_cartesian_b.cpp —— 哑铃/山脊/平行坐标/马赛克/QQ/子弹/迷你/流图 端到端绘制
//
// 只断言结构事实（字符出现、几何落位、退化输入不崩），不锁逐字节外观。
#include <doctest.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/bullet.hpp"
#include "termchart/chart/cartesian/dumbbell.hpp"
#include "termchart/chart/cartesian/mosaic.hpp"
#include "termchart/chart/cartesian/parallel.hpp"
#include "termchart/chart/cartesian/qq.hpp"
#include "termchart/chart/cartesian/ridgeline.hpp"
#include "termchart/chart/cartesian/sparkline.hpp"
#include "termchart/chart/cartesian/streamgraph.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/core/point.hpp"

namespace {

using namespace termchart;
using namespace termchart::chart;

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

std::size_t count_nonblank(const canvas::Canvas& c) {
    return static_cast<std::size_t>(c.width()) * c.height() - count_char(c, U'\u0020');
}

}  // namespace

// ---------- 哑铃图 ----------

TEST_CASE("chart/dumbbell: 每行两枚端点标记 + 内框落位") {
    canvas::Canvas canvas(60, 20);
    DumbbellChart chart;
    chart.set_rows({{"a", 1.0, 4.0}, {"b", 2.0, 6.0}, {"c", 0.0, 3.0}});
    chart.draw({canvas});

    CHECK(count_nonblank(canvas) > 10);
    CHECK(count_char(canvas, U'\u25CF') >= 6);  // 3 行 × 2 端点
    CHECK(count_char(canvas, U'\u2022') >= 3);  // 行内连线
    const Rect inner = chart.last_area().inner_rect();
    CHECK(inner.x == doctest::Approx(8.0));
    CHECK(inner.bottom() == doctest::Approx(17.0));
}

TEST_CASE("chart/dumbbell: 空行集与非有限值不绘制也不崩") {
    canvas::Canvas canvas(30, 10);
    DumbbellChart empty;
    empty.draw({canvas});
    CHECK(count_nonblank(canvas) == 0);

    DumbbellChart nan_chart;
    nan_chart.set_rows({{"x", std::nan(""), 1.0}, {"y", 0.0, 2.0}});
    canvas::Canvas canvas2(30, 10);
    nan_chart.draw({canvas2});  // 含 NaN 的行被剔除，另一行正常画
    CHECK(count_nonblank(canvas2) > 0);
}

// ---------- 山脊图 ----------

TEST_CASE("chart/ridgeline: 多组密度形成可见山脊") {
    canvas::Canvas canvas(60, 20);
    RidgelineChart chart;
    RidgelineChart::Group g1{"low", {1.0, 1.5, 2.0, 2.5, 3.0, 2.0, 1.8}, Color::ansi16(4)};
    RidgelineChart::Group g2{"mid", {2.0, 2.5, 3.0, 3.5, 4.0, 3.2, 2.8}, Color::ansi16(2)};
    RidgelineChart::Group g3{"high", {3.0, 3.5, 4.0, 4.5, 5.0, 4.0, 3.6}, Color::ansi16(3)};
    chart.set_groups({g1, g2, g3});
    chart.draw({canvas});

    CHECK(count_nonblank(canvas) > 20);
    CHECK(count_char(canvas, U'\u2588') > 10);  // 密度主体是实心块
}

TEST_CASE("chart/ridgeline: 空组不绘制") {
    canvas::Canvas canvas(40, 12);
    RidgelineChart chart;
    chart.set_groups({{"empty", {}, Color::ansi16(4)}});
    chart.draw({canvas});
    CHECK(count_nonblank(canvas) == 0);
}

// ---------- 平行坐标 ----------

TEST_CASE("chart/parallel: 每维一条竖轴 + 观测折线") {
    canvas::Canvas canvas(60, 20);
    ParallelChart chart;
    chart.set_axes({"a", "b", "c"}, {{1.0, 2.0, 3.0}, {3.0, 2.0, 1.0}});
    chart.draw({canvas});

    CHECK(count_nonblank(canvas) > 10);
    CHECK(count_char(canvas, U'\u2502') >= 10);  // 3 条竖轴
    CHECK(count_char(canvas, U'\u2022') >= 4);   // 两条观测折线
    const Rect inner = chart.last_area().inner_rect();
    CHECK(inner.w == doctest::Approx(51.0));
}

TEST_CASE("chart/parallel: 维度不足或行长不匹配不绘制") {
    canvas::Canvas canvas(40, 12);
    ParallelChart one_axis;
    one_axis.set_axes({"only"}, {{1.0}, {2.0}});
    one_axis.draw({canvas});
    CHECK(count_nonblank(canvas) == 0);

    ParallelChart jagged;
    jagged.set_axes({"a", "b"}, {{1.0, 2.0, 3.0}});  // 长度与维度不符
    canvas::Canvas canvas2(40, 12);
    jagged.draw({canvas2});
    // 竖轴是结构性元素仍会画，但有效观测为 0 → 不得出现任何观测折线。
    CHECK(count_char(canvas2, U'\u2022') == 0);
}

// ---------- 马赛克图 ----------

TEST_CASE("chart/mosaic: 列宽按边际、块按行占比堆叠") {
    canvas::Canvas canvas(60, 20);
    MosaicChart chart;
    chart.set_counts({{3.0, 1.0}, {1.0, 3.0}}, {"r0", "r1"}, {"c0", "c1"});
    chart.draw({canvas});

    CHECK(count_nonblank(canvas) > 20);
    CHECK(count_char(canvas, U'\u2588') > 20);
    const Rect inner = chart.last_area().inner_rect();
    // 底行最左格应是第 0 行类别的实心块（自下而上堆叠）。
    CHECK(canvas.at(static_cast<std::uint32_t>(inner.x) + 2,
                    static_cast<std::uint32_t>(inner.bottom()) - 1)
              .cp == U'\u2588');
}

TEST_CASE("chart/mosaic: 锯齿矩阵与全零矩阵不绘制") {
    canvas::Canvas canvas(40, 12);
    MosaicChart jagged;
    jagged.set_counts({{1.0, 2.0}, {1.0}}, {}, {});
    jagged.draw({canvas});
    CHECK(count_nonblank(canvas) == 0);

    MosaicChart zeros;
    zeros.set_counts({{0.0, 0.0}, {0.0, 0.0}}, {}, {});
    canvas::Canvas canvas2(40, 12);
    zeros.draw({canvas2});
    CHECK(count_nonblank(canvas2) == 0);
}

// ---------- QQ 图 ----------

TEST_CASE("chart/qq: 样本点落格且参考线存在") {
    canvas::Canvas canvas(60, 20);
    QQChart chart;
    chart.set_sample({0.1, 0.5, 1.2, 0.9, 2.0, 1.6, 0.3, 1.0, 0.7, 1.4});
    chart.draw({canvas});

    CHECK(count_nonblank(canvas) > 10);
    CHECK(count_char(canvas, U'\u25CF') >= 5);   // 10 个样本点（允许个别重合）
    CHECK(count_char(canvas, U'\u2022') >= 5);   // 参考线
    const Rect inner = chart.last_area().inner_rect();
    CHECK(inner.bottom() == doctest::Approx(17.0));
}

TEST_CASE("chart/qq: 空样本与非有限值不崩") {
    canvas::Canvas canvas(40, 12);
    QQChart empty;
    empty.draw({canvas});
    CHECK(count_nonblank(canvas) == 0);

    QQChart nan_sample;
    nan_sample.set_sample({1.0, std::nan(""), 2.0});
    canvas::Canvas canvas2(40, 12);
    nan_sample.draw({canvas2});
    CHECK(count_nonblank(canvas2) > 0);  // NaN 剔除后仍正常画
}

// ---------- 子弹图 ----------

TEST_CASE("chart/bullet: 背景带/度量条/目标线三种字符齐备") {
    canvas::Canvas canvas(60, 20);
    BulletChart chart;
    chart.set_rows({{"a", 50.0, 70.0, {30.0, 60.0, 100.0}},
                    {"b", 20.0, 40.0, {25.0, 50.0, 80.0}}});
    chart.draw({canvas});

    CHECK(count_char(canvas, U'\u2588') > 0);    // 度量条
    CHECK(count_char(canvas, U'\u2502') > 0);    // 目标线
    CHECK((count_char(canvas, U'\u2591') + count_char(canvas, U'\u2592') +
           count_char(canvas, U'\u2593')) > 0);  // 背景带
}

TEST_CASE("chart/bullet: 空行集不绘制") {
    canvas::Canvas canvas(40, 12);
    BulletChart chart;
    chart.draw({canvas});
    CHECK(count_nonblank(canvas) == 0);
}

// ---------- 迷你图 ----------

TEST_CASE("chart/sparkline: 单行铺满且低值列不空白") {
    canvas::Canvas canvas(30, 3);
    SparklineChart chart;
    chart.set_values({1.0, 3.0, 2.0, 5.0, 4.0, 2.5, 3.5, 1.5});
    chart.draw({canvas});

    // 第 0 行所有列都非空格（含最低值也有 1/8 块）。
    for (std::uint32_t x = 0; x < canvas.width(); ++x) {
        CHECK(canvas.at(x, 0).cp != U'\u0020');
    }
    // 末列是最新值标记 ●。
    CHECK(canvas.at(canvas.width() - 1, 0).cp == U'\u25CF');
}

TEST_CASE("chart/sparkline: 空数据与常值序列不崩") {
    canvas::Canvas canvas(20, 2);
    SparklineChart empty;
    empty.draw({canvas});
    CHECK(count_nonblank(canvas) == 0);

    SparklineChart flat;
    flat.set_values({5.0, 5.0, 5.0});  // 常值：span 为 0，取中档
    canvas::Canvas canvas2(20, 2);
    flat.draw({canvas2});
    CHECK(count_nonblank(canvas2) > 0);
}

// ---------- 流图 ----------

TEST_CASE("chart/streamgraph: 多层堆叠铺满内框") {
    canvas::Canvas canvas(60, 20);
    StreamgraphChart chart;
    Series lower;
    lower.name = "base";
    lower.color = Color::ansi16(4);
    Series upper;
    upper.name = "extra";
    upper.color = Color::ansi16(2);
    for (int i = 0; i < 10; ++i) {
        const double x = static_cast<double>(i);
        lower.points.push_back(Point{x, 2.0 + 0.1 * static_cast<double>(i % 3)});
        upper.points.push_back(Point{x, 1.0 + 0.05 * static_cast<double>(i % 4)});
    }
    chart.add_series(std::move(lower));
    chart.add_series(std::move(upper));
    chart.draw({canvas});

    CHECK(count_char(canvas, U'\u2588') > 50);  // 河体由实心块构成
    const Rect inner = chart.last_area().inner_rect();
    CHECK(inner.bottom() == doctest::Approx(17.0));
}

TEST_CASE("chart/streamgraph: 无层不绘制") {
    canvas::Canvas canvas(40, 12);
    StreamgraphChart chart;
    chart.draw({canvas});
    CHECK(count_nonblank(canvas) == 0);
}
