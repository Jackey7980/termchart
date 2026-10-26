// test_chart_animation.cpp —— Chart 层按名动画（进度表 + 折线/柱状接入）
//
// 口径：动画是**图表层**的事，不进 Scene、不进渲染管线。本文件只验证三件事：
//   1. 进度表本身（未登记/未 tick 的安全默认值、延迟、缓动、同名覆盖）；
//   2. 折线"从左向右"、柱"从基线生长"的可见结果；
//   3. **域不随进度变**——否则画面会一边长一边缩放。
#include <doctest.h>

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/bar_chart.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/chart/chart.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"

namespace {

using namespace termchart;
using namespace termchart::chart;

std::size_t count_cp(const canvas::Canvas& canvas, char32_t cp) {
    std::size_t n = 0;
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            if (canvas.at(x, y).cp == cp) {
                ++n;
            }
        }
    }
    return n;
}

std::vector<char32_t> snapshot(const canvas::Canvas& canvas) {
    std::vector<char32_t> out;
    out.reserve(static_cast<std::size_t>(canvas.width()) * canvas.height());
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            out.push_back(canvas.at(x, y).cp);
        }
    }
    return out;
}

Series make_series(std::string name, std::vector<Point> points) {
    Series s;
    s.name = std::move(name);
    s.color = Color::ansi16(2);
    s.points = std::move(points);
    return s;
}

}  // namespace

TEST_CASE("chart/anim: 未登记动画时进度恒为 1，绘制完全不变") {
    LineChart chart;
    chart.add_series(make_series("a", {{0.0, 0.0}, {1.0, 1.0}, {2.0, 4.0}}));
    CHECK(chart.animation_count() == 0);
    CHECK(chart.progress_of("a") == doctest::Approx(1.0));
    CHECK(chart.progress_of("没这个名字") == doctest::Approx(1.0));
    CHECK_FALSE(chart.animating());

    // 连 tick 都不调用（静态渲染路径）：两次绘制必须逐格相同。
    canvas::Canvas c1(40, 12);
    canvas::Canvas c2(40, 12);
    chart.draw(DrawContext{c1});
    chart.draw(DrawContext{c2});
    CHECK(snapshot(c1) == snapshot(c2));
    CHECK(count_cp(c1, kMarkerChar) == 3);
}

TEST_CASE("chart/anim: 登记了但从未 tick，仍然画全图（不白屏）") {
    LineChart chart;
    chart.add_series(make_series("a", {{0.0, 0.0}, {1.0, 1.0}, {2.0, 4.0}}));
    chart.add_animation("a", 1.0, Easing::Linear);
    CHECK(chart.animation_count() == 1);
    // 关键安全阀：忘了 tick 不该让图消失。
    CHECK(chart.progress_of("a") == doctest::Approx(1.0));
    CHECK_FALSE(chart.animating());

    canvas::Canvas canvas(40, 12);
    chart.draw(DrawContext{canvas});
    CHECK(count_cp(canvas, kMarkerChar) == 3);
}

TEST_CASE("chart/anim: 首帧即起点，进度按时间推进") {
    LineChart chart;
    chart.add_animation("a", 0.5, Easing::Linear);

    chart.tick(10.0);  // 首帧 = 起点
    CHECK(chart.progress_of("a") == doctest::Approx(0.0));
    CHECK(chart.animating());

    chart.tick(10.25);
    CHECK(chart.progress_of("a") == doctest::Approx(0.5));

    chart.tick(10.5);
    CHECK(chart.progress_of("a") == doctest::Approx(1.0));
    CHECK_FALSE(chart.animating());

    chart.tick(99.0);  // 越过终点继续 tick 不越界
    CHECK(chart.progress_of("a") == doctest::Approx(1.0));
}

TEST_CASE("chart/anim: 延迟区间内停在 0，结束后重新计时") {
    LineChart chart;
    chart.add_animation("a", 1.0, Easing::Linear, 2.0);  // 等 2 秒，跑 1 秒

    chart.tick(0.0);
    chart.tick(1.9);
    CHECK(chart.progress_of("a") == doctest::Approx(0.0));
    CHECK(chart.animating());

    chart.tick(2.5);  // 已跑 0.5 秒
    CHECK(chart.progress_of("a") == doctest::Approx(0.5));

    chart.tick(3.0);
    CHECK(chart.progress_of("a") == doctest::Approx(1.0));
}

TEST_CASE("chart/anim: 零时长立即完成，非有限时间被忽略") {
    LineChart chart;
    chart.add_animation("a", 0.0, Easing::Linear);
    chart.tick(5.0);
    CHECK(chart.progress_of("a") == doctest::Approx(1.0));
    CHECK_FALSE(chart.animating());

    LineChart other;
    other.add_animation("a", 1.0, Easing::Linear);
    other.tick(7.0);
    const double before = other.progress_of("a");
    other.tick(std::nan(""));  // 脏时钟：不推进、不污染
    CHECK(other.progress_of("a") == doctest::Approx(before));
}

TEST_CASE("chart/anim: 同名重复登记是覆盖，不是叠加") {
    LineChart chart;
    chart.add_animation("a", 1.0, Easing::Linear);
    chart.add_animation("a", 0.25, Easing::Linear);
    CHECK(chart.animation_count() == 1);

    chart.tick(0.0);
    chart.tick(0.25);
    CHECK(chart.progress_of("a") == doctest::Approx(1.0));  // 按 0.25 秒那条算
}

TEST_CASE("chart/anim: restart 把当前时刻当新起点") {
    LineChart chart;
    chart.add_animation("a", 1.0, Easing::Linear);
    chart.tick(0.0);
    chart.tick(1.0);
    CHECK(chart.progress_of("a") == doctest::Approx(1.0));

    chart.restart_animations(50.0);
    CHECK(chart.progress_of("a") == doctest::Approx(0.0));
    chart.tick(50.5);
    CHECK(chart.progress_of("a") == doctest::Approx(0.5));
}

TEST_CASE("chart/anim: 折线从左向右揭开，域不变") {
    canvas::Canvas full(50, 14);
    LineChart reference;
    reference.add_series(make_series("a", {{0.0, 0.0}, {1.0, 1.0}, {2.0, 4.0}, {3.0, 9.0}}));
    reference.draw(DrawContext{full});
    const std::size_t full_markers = count_cp(full, kMarkerChar);
    CHECK(full_markers == 4);

    LineChart chart;
    chart.add_series(make_series("a", {{0.0, 0.0}, {1.0, 1.0}, {2.0, 4.0}, {3.0, 9.0}}));
    chart.add_animation("a", 1.0, Easing::Linear);

    canvas::Canvas start(50, 14);
    chart.tick(0.0);
    chart.draw(DrawContext{start});
    CHECK(count_cp(start, kMarkerChar) == 0);  // 起点：只剩轴和网格

    canvas::Canvas half(50, 14);
    chart.tick(0.5);
    chart.draw(DrawContext{half});
    const std::size_t half_markers = count_cp(half, kMarkerChar);
    CHECK(half_markers >= 1);
    CHECK(half_markers < full_markers);

    // 域不随进度变：半程与整程的 y 轴刻度文本必须一致（比较左栏）。
    for (std::uint32_t y = 0; y < half.height(); ++y) {
        for (std::uint32_t x = 0; x < 6; ++x) {
            CHECK(half.at(x, y).cp == full.at(x, y).cp);
        }
    }

    canvas::Canvas done(50, 14);
    chart.tick(1.0);
    chart.draw(DrawContext{done});
    CHECK(count_cp(done, kMarkerChar) == full_markers);
    CHECK(snapshot(done) == snapshot(full));  // 动画结束 == 无动画
}

TEST_CASE("chart/anim: 柱从基线生长，负值向下长") {
    // 图例也用 █ 画色块，会和柱子混在一起计数——统计柱体时先关掉图例。
    BarChart reference;
    reference.set_values({3.0, -2.0, 5.0}, {"a", "b", "c"}, Color::ansi16(2), "sales");
    reference.set_show_legend(false);
    canvas::Canvas full(50, 14);
    reference.draw(DrawContext{full});
    const std::size_t full_bars = count_cp(full, kBarChar);
    CHECK(full_bars > 0);

    BarChart chart;
    chart.set_values({3.0, -2.0, 5.0}, {"a", "b", "c"}, Color::ansi16(2), "sales");
    chart.set_show_legend(false);
    chart.add_animation("sales", 1.0, Easing::Linear);

    canvas::Canvas start(50, 14);
    chart.tick(0.0);
    chart.draw(DrawContext{start});
    CHECK(count_cp(start, kBarChar) == 0);

    canvas::Canvas half(50, 14);
    chart.tick(0.5);
    chart.draw(DrawContext{half});
    const std::size_t half_bars = count_cp(half, kBarChar);
    CHECK(half_bars > 0);
    CHECK(half_bars < full_bars);

    canvas::Canvas done(50, 14);
    chart.tick(1.0);
    chart.draw(DrawContext{done});
    CHECK(snapshot(done) == snapshot(full));
}

TEST_CASE("chart/anim: 只登记了部分系列时，没登记的照样全画") {
    LineChart chart;
    chart.add_series(make_series("动画线", {{0.0, 0.0}, {1.0, 1.0}}));
    chart.add_series(make_series("静态线", {{0.0, 1.0}, {1.0, 0.0}}));
    chart.add_animation("动画线", 1.0, Easing::Linear);

    chart.tick(0.0);
    CHECK(chart.progress_of("动画线") == doctest::Approx(0.0));
    CHECK(chart.progress_of("静态线") == doctest::Approx(1.0));

    canvas::Canvas canvas(40, 12);
    chart.draw(DrawContext{canvas});
    // 静态线两个点都在；动画线一个都没画 → 恰好 2 个 marker。
    CHECK(count_cp(canvas, kMarkerChar) == 2);
}
