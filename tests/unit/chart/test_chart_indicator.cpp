// test_chart_indicator.cpp —— gauge_kpi/progress_ring/star_rating/status_light/
// thermometer 端到端绘制（结构事实断言，不做逐字节 golden）
#include <doctest.h>

#include <cmath>
#include <cstdint>
#include <limits>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/indicator/gauge_kpi.hpp"
#include "termchart/chart/indicator/progress_ring.hpp"
#include "termchart/chart/indicator/star_rating.hpp"
#include "termchart/chart/indicator/status_light.hpp"
#include "termchart/chart/indicator/thermometer.hpp"
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

constexpr char32_t kArc = U'\u25CF';       // ● 已完成弧/状态灯
constexpr char32_t kTrack = U'\u00B7';     // · 未完成轨道
constexpr char32_t kFull = U'\u2605';      // ★ 满星
constexpr char32_t kHalf = U'\u25D0';      // ◐ 半星
constexpr char32_t kStarEmpty = U'\u2606'; // ☆ 空星
constexpr char32_t kBlock = U'\u2588';     // █ 液柱/球泡
constexpr char32_t kWall = U'\u2502';      // │ 管壁
constexpr char32_t kTick = U'\u2500';      // ─ 刻度
constexpr char32_t kUp = U'\u25B2';        // ▲ 变化量
constexpr char32_t kDown = U'\u25BC';      // ▼ 变化量
constexpr char32_t kCornerTl = U'\u250C';  // ┌
constexpr char32_t kCornerTr = U'\u2510';  // ┐
constexpr char32_t kCornerBl = U'\u2514';  // └
constexpr char32_t kCornerBr = U'\u2518';  // ┘

}  // namespace

// ---------------- gauge_kpi ----------------

TEST_CASE("chart/indicator/gauge_kpi: 边框、主值与变化量齐备") {
    canvas::Canvas canvas(30, 10);  // 内容区 {8,1,21,6}
    GaugeKpi chart;
    chart.set_label("UPTIME");
    chart.set_value("99.9%");
    chart.set_delta("5%", true);
    chart.draw({canvas});

    // 四角完整。
    CHECK(canvas.at(8, 1).cp == kCornerTl);
    CHECK(canvas.at(28, 1).cp == kCornerTr);
    CHECK(canvas.at(8, 6).cp == kCornerBl);
    CHECK(canvas.at(28, 6).cp == kCornerBr);
    // 主值文本与上升箭头。
    CHECK(count_char(canvas, U'9') == 3);
    CHECK(count_char(canvas, kUp) == 1);
    CHECK(count_char(canvas, kDown) == 0);
}

TEST_CASE("chart/indicator/gauge_kpi: 关闭变化量、空卡片与小画布退化") {
    canvas::Canvas c1(30, 10);
    GaugeKpi nodelta;
    GaugeKpiOptions nodelta_opts;
    nodelta_opts.show_delta = false;
    nodelta.set_options(nodelta_opts);
    nodelta.set_label("x");
    nodelta.set_value("1");
    nodelta.set_delta("9%", false);
    nodelta.draw({c1});
    CHECK(count_char(c1, kDown) == 0);
    CHECK(count_char(c1, kUp) == 0);

    // 全空 → 只有边框。
    canvas::Canvas c2(30, 10);
    GaugeKpi bare;
    bare.draw({c2});
    CHECK(count_char(c2, kCornerTl) == 1);

    // 画布过小：退化为把主值贴左上画出。
    canvas::Canvas c3(11, 5);
    GaugeKpi tiny;
    tiny.set_value("OK");
    tiny.draw({c3});
    CHECK(c3.at(8, 1).cp == U'O');
    CHECK(count_nonblank(c3) == 2);
}

// ---------------- progress_ring ----------------

TEST_CASE("chart/indicator/progress_ring: 弧段与轨道同时存在且中心有读数") {
    canvas::Canvas canvas(20, 10);  // 内容区 {8,1,11,6}
    ProgressRing chart;
    chart.set_fraction(0.75);
    chart.draw({canvas});

    CHECK(count_char(canvas, kArc) >= 8);
    CHECK(count_char(canvas, kTrack) >= 1);
    CHECK(count_char(canvas, U'%') == 1);  // 中心 75%
}

TEST_CASE("chart/indicator/progress_ring: 0/1/越界/非有限收敛不崩") {
    // fraction 0：全是轨道、无弧。
    canvas::Canvas c1(20, 10);
    ProgressRing zero;
    zero.set_fraction(0.0);
    zero.draw({c1});
    CHECK(count_char(c1, kArc) == 0);
    CHECK(count_char(c1, kTrack) > 0);
    CHECK(count_char(c1, U'%') == 1);  // 0%

    // fraction 1：满环无轨道。
    canvas::Canvas c2(20, 10);
    ProgressRing full;
    full.set_fraction(1.0);
    full.draw({c2});
    CHECK(count_char(c2, kTrack) == 0);
    CHECK(count_char(c2, kArc) > 0);

    // 越界与非有限按收敛处理。
    canvas::Canvas c3(20, 10);
    ProgressRing weird;
    weird.set_fraction(42.0);
    weird.draw({c3});
    CHECK(count_char(c3, kTrack) == 0);  // 收敛到 1 → 满环

    canvas::Canvas c4(20, 10);
    ProgressRing nan;
    nan.set_fraction(std::numeric_limits<double>::quiet_NaN());
    nan.draw({c4});
    CHECK(count_char(c4, kArc) == 0);  // 收敛到 0

    // 关闭百分比读数。
    canvas::Canvas c5(20, 10);
    ProgressRing nopct;
    ProgressRingOptions nopct_opts;
    nopct_opts.show_percent = false;
    nopct.set_options(nopct_opts);
    nopct.set_fraction(0.5);
    nopct.draw({c5});
    CHECK(count_char(c5, U'%') == 0);
    CHECK(count_char(c5, kArc) > 0);
}

// ---------------- star_rating ----------------

TEST_CASE("chart/indicator/star_rating: 半星四舍五入、满/半/空各就各位") {
    canvas::Canvas canvas(20, 10);
    StarRating chart;
    chart.set_rating(3.5);
    chart.draw({canvas});

    CHECK(count_char(canvas, kFull) == 3);
    CHECK(count_char(canvas, kHalf) == 1);
    CHECK(count_char(canvas, kStarEmpty) == 1);
}

TEST_CASE("chart/indicator/star_rating: 评分收敛与星数收敛") {
    // 0 分全空星。
    canvas::Canvas c1(20, 10);
    StarRating zero;
    zero.set_rating(0.0);
    zero.draw({c1});
    CHECK(count_char(c1, kStarEmpty) == 5);
    CHECK(count_char(c1, kFull) == 0);

    // 超上限与负值都收敛。
    canvas::Canvas c2(20, 10);
    StarRating over;
    over.set_rating(100.0);
    over.draw({c2});
    CHECK(count_char(c2, kFull) == 5);

    canvas::Canvas c3(20, 10);
    StarRating neg;
    neg.set_rating(-2.0);
    neg.draw({c3});
    CHECK(count_char(c3, kStarEmpty) == 5);

    // NaN 按 0。
    canvas::Canvas c4(20, 10);
    StarRating nan;
    nan.set_rating(std::numeric_limits<double>::quiet_NaN());
    nan.draw({c4});
    CHECK(count_char(c4, kStarEmpty) == 5);

    // max_stars 收敛到 [1, 20]。
    canvas::Canvas c5(20, 10);
    StarRating three;
    StarRatingOptions opts;
    opts.max_stars = 3;
    three.set_options(opts);
    three.set_rating(2.5);
    three.draw({c5});
    CHECK(count_char(c5, kFull) == 2);
    CHECK(count_char(c5, kHalf) == 1);
    CHECK(count_char(c5, kStarEmpty) == 0);
}

// ---------------- status_light ----------------

TEST_CASE("chart/indicator/status_light: 单格 ● 与标签齐备") {
    canvas::Canvas canvas(20, 10);
    StatusLight chart;
    chart.set_state(LightState::Ok);
    chart.set_label("ok");
    chart.draw({canvas});

    CHECK(count_char(canvas, kArc) == 1);
    CHECK(canvas.at(8, 3).cp == kArc);   // 内容区垂直居中行
    CHECK(canvas.at(10, 3).cp == U'o');  // 标签从 ● 右侧 2 格起
}

TEST_CASE("chart/indicator/status_light: 四状态与关标签不崩") {
    for (const LightState state :
         {LightState::Ok, LightState::Warning, LightState::Error,
          LightState::Unknown}) {
        canvas::Canvas canvas(20, 10);
        StatusLight chart;
        chart.set_state(state);
        REQUIRE_NOTHROW(chart.draw({canvas}));
        CHECK(count_char(canvas, kArc) == 1);
    }

    // 关标签 / 空标签：只剩一粒灯。
    canvas::Canvas c2(20, 10);
    StatusLight nolabel;
    StatusLightOptions nolabel_opts;
    nolabel_opts.show_label = false;
    nolabel.set_options(nolabel_opts);
    nolabel.set_label("ignored");
    nolabel.draw({c2});
    CHECK(count_nonblank(c2) == 1);

    canvas::Canvas c3(20, 10);
    StatusLight nolabel2;
    nolabel2.draw({c3});
    CHECK(count_nonblank(c3) == 1);
}

// ---------------- thermometer ----------------

TEST_CASE("chart/indicator/thermometer: 管壁、液柱、球泡与刻度齐备") {
    canvas::Canvas canvas(20, 12);  // 内容区 {8,1,11,8}
    Thermometer chart;
    chart.set_value(50.0);  // vmin 0 / vmax 100 → 半管
    chart.draw({canvas});

    // 管壁两列 × 7 行 = 14 格，其中右壁 5 行被刻度 ─ 覆盖 → 剩 9。
    CHECK(count_char(canvas, kWall) == 9);
    // 液柱 3 格 + 球泡 3 格。
    CHECK(count_char(canvas, kBlock) == 6);
    CHECK(count_char(canvas, kTick) == 5);
    // 液柱在管中央列。
    CHECK(canvas.at(13, 7).cp == kBlock);
    CHECK(canvas.at(13, 5).cp == kBlock);
}

TEST_CASE("chart/indicator/thermometer: NaN 值只画空管、域颠倒交换") {
    // NaN → 无液柱，只有球泡 3 格 █；管内中央列保持空白。
    canvas::Canvas c1(20, 12);
    Thermometer nan;
    nan.set_value(std::numeric_limits<double>::quiet_NaN());
    nan.draw({c1});
    CHECK(count_char(c1, kBlock) == 3);
    CHECK(c1.at(12, 5).cp == kWall);        // 左壁仍在
    CHECK(c1.at(13, 5).cp == U'\u0020');    // 管内无液柱

    // vmin > vmax 按交换处理：50 仍在域内。
    canvas::Canvas c2(20, 12);
    Thermometer swapped;
    ThermometerOptions opts;
    opts.vmin = 100.0;
    opts.vmax = 0.0;
    swapped.set_options(opts);
    swapped.set_value(50.0);
    REQUIRE_NOTHROW(swapped.draw({c2}));
    CHECK(count_char(c2, kBlock) == 6);

    // 低于下界 → 液柱为 0 格。
    canvas::Canvas c3(20, 12);
    Thermometer below;
    below.set_value(-10.0);
    below.draw({c3});
    CHECK(count_char(c3, kBlock) == 3);

    // 画布过小不绘制。
    canvas::Canvas c4(10, 4);
    Thermometer tiny;
    tiny.set_value(50.0);
    REQUIRE_NOTHROW(tiny.draw({c4}));
    CHECK(count_nonblank(c4) == 0);
}

TEST_CASE("chart/indicator/thermometer: 刻度数收敛") {
    canvas::Canvas canvas(20, 12);
    Thermometer chart;
    ThermometerOptions opts;
    opts.ticks = 0;
    chart.set_options(opts);
    chart.set_value(50.0);
    chart.draw({canvas});
    CHECK(count_char(canvas, kTick) == 0);
    CHECK(count_char(canvas, kBlock) == 6);
}
