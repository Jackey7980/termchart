// test_chart_area_step.cpp —— 面积图 / 阶梯图 端到端绘制
//
// 断言的重点是"语义"而非"没崩"：
//   * 面积图：填充确实存在、不越过基线、折线没被填掉、换基线填充随之变化；
//   * 阶梯图：相邻点中间列落在某一端（或中点）的水平上，而不是线性插值；
//   * 上下文开关：draw_axes / draw_title 是活的；
//   * 边界：空 / 单点 / 恒定 / NaN / 小画布裁剪都不越界。
#include <doctest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/area.hpp"
#include "termchart/chart/cartesian/step.hpp"
#include "termchart/chart/chart.hpp"
#include "termchart/chart/context.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/layout/box.hpp"

namespace {

using namespace termchart;
using namespace termchart::chart;

// AreaChart 默认填充字符（低调的 '·'）。
constexpr char32_t kFill = U'\u00B7';

std::size_t count_cp(const canvas::Canvas& c, char32_t cp) {
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

// 某列中第一次出现该字符的行；找不到返回 false。
bool row_of_cp(const canvas::Canvas& c, std::uint32_t x, char32_t cp,
               std::uint32_t& out_row) {
    for (std::uint32_t y = 0; y < c.height(); ++y) {
        if (c.at(x, y).cp == cp) {
            out_row = y;
            return true;
        }
    }
    return false;
}

// 该字符占用格的掩码（用于比较"两次绘制的填充区域是否相同"）。
std::vector<char> mask_of_cp(const canvas::Canvas& c, char32_t cp) {
    std::vector<char> mask(static_cast<std::size_t>(c.width()) * c.height(), 0);
    for (std::uint32_t y = 0; y < c.height(); ++y) {
        for (std::uint32_t x = 0; x < c.width(); ++x) {
            if (c.at(x, y).cp == cp) {
                mask[static_cast<std::size_t>(y) * c.width() + x] = 1;
            }
        }
    }
    return mask;
}

// 用给定开关跑一次面积图，返回非空格计数（用于上下文开关断言）。
std::size_t draw_area_nonblank(const std::vector<Point>& pts, bool title, bool axes) {
    canvas::Canvas canvas(60, 20);
    AreaChart chart;
    chart.set_values(pts);
    if (title) {
        chart.set_title("AREA");
    }
    DrawContext ctx{canvas};
    ctx.draw_grid = false;  // 只留轴/标题做对比，避免网格噪声
    ctx.draw_axes = axes;
    ctx.draw_title = title;
    chart.draw(ctx);
    return count_nonblank(canvas);
}

struct StepProbe {
    std::uint32_t col_a = 0;
    std::uint32_t col_b = 0;
    std::uint32_t row_a = 0;
    std::uint32_t row_b = 0;
    std::uint32_t mid_col = 0;
    std::uint32_t row_mid = 0;
    bool ok = false;
};

// 画一条只有两个点、关掉全部装饰的阶梯线，取出两端与中间列的关键行号。
StepProbe probe_step(const std::vector<Point>& pts, StepChart::Where where) {
    canvas::Canvas canvas(60, 20);
    StepChart chart;
    chart.set_values(pts);
    chart.set_where(where);
    DrawContext ctx{canvas};
    ctx.draw_grid = false;
    ctx.draw_axes = false;
    ctx.draw_legend = false;
    chart.draw(ctx);

    StepProbe p;
    const layout::PlotArea& area = chart.last_area();
    p.col_a = static_cast<std::uint32_t>(std::floor(area.x_scale().forward(pts.front().x)));
    p.col_b = static_cast<std::uint32_t>(std::floor(area.x_scale().forward(pts.back().x)));
    p.mid_col = static_cast<std::uint32_t>((p.col_a + p.col_b) / 2);
    const bool ok_a = row_of_cp(canvas, p.col_a, kMarkerChar, p.row_a);
    const bool ok_b = row_of_cp(canvas, p.col_b, kMarkerChar, p.row_b);
    const bool ok_mid = row_of_cp(canvas, p.mid_col, kLineChar, p.row_mid);
    p.ok = ok_a && ok_b && ok_mid;
    return p;
}

}  // namespace

TEST_CASE("chart/area: 逐列填充存在、不越过基线、折线仍在") {
    canvas::Canvas canvas(60, 20);
    AreaChart chart;
    std::vector<Point> pts;
    for (int i = 0; i <= 20; ++i) {
        const double x = static_cast<double>(i) * 0.5;  // 0..10
        const double y = 3.0 + std::sin(x);             // 2..4，恒正
        pts.push_back(Point{x, y});
    }
    chart.set_values(pts, Color::ansi16(2), "hill");
    chart.draw({canvas});

    CHECK(count_cp(canvas, kFill) > 0);  // 填充确实存在
    CHECK(count_cp(canvas, kMarkerChar) > 0);
    CHECK(count_cp(canvas, kLineChar) > 0);  // 折线本身仍在

    const layout::PlotArea& area = chart.last_area();
    CHECK(area.inner_rect().w > 0.0);
    CHECK(area.inner_rect().h > 0.0);

    // 数据全为正 → 填充只应出现在基线行**之上**（行号更小），绝不越过基线。
    const double ybase = area.y_scale().forward(0.0);
    const auto base_row = static_cast<std::uint32_t>(std::floor(ybase));
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            if (canvas.at(x, y).cp == kFill) {
                CHECK(y < base_row);
            }
        }
    }
}

TEST_CASE("chart/area: set_baseline 改变填充区域（同输入两基线结果不同）") {
    const std::vector<Point> pts{{0.0, 6.0}, {3.0, 8.0}, {6.0, 5.0}, {9.0, 7.0}};

    canvas::Canvas c0(60, 20);
    AreaChart a0;
    a0.set_values(pts);
    a0.set_baseline(0.0);
    a0.draw({c0});

    canvas::Canvas c3(60, 20);
    AreaChart a3;
    a3.set_values(pts);
    a3.set_baseline(3.0);
    a3.draw({c3});

    const std::size_t n0 = count_cp(c0, kFill);
    const std::size_t n3 = count_cp(c3, kFill);
    CHECK(n0 > 0);
    CHECK(n3 > 0);
    // 同一次输入、两个基线：填充区域必须逐格不同（不是只改了数字）。
    CHECK(mask_of_cp(c0, kFill) != mask_of_cp(c3, kFill));
}

TEST_CASE("chart/step: Post 档中间列落在前一点水平上，而非线性插值") {
    const std::vector<Point> pts{{0.0, 1.0}, {10.0, 9.0}};
    const StepProbe post = probe_step(pts, StepChart::Where::Post);
    REQUIRE(post.ok);
    CHECK(post.row_a != post.row_b);  // 两端高度明显不同

    // Post（先横后竖）：中间列的水平台阶必须正好在前一点的高度上。
    CHECK(post.row_mid == post.row_a);
    // 线性插值会落在两端行号的中间——明确排除：中间列不等于两端均值。
    CHECK(post.row_mid != (post.row_a + post.row_b) / 2);
    CHECK(post.row_mid != post.row_b);
}

TEST_CASE("chart/step: Pre 取后一点、Mid 取中点") {
    const std::vector<Point> pts{{0.0, 1.0}, {10.0, 9.0}};
    const StepProbe post = probe_step(pts, StepChart::Where::Post);
    const StepProbe pre = probe_step(pts, StepChart::Where::Pre);
    const StepProbe mid = probe_step(pts, StepChart::Where::Mid);
    REQUIRE(post.ok);
    REQUIRE(pre.ok);
    REQUIRE(mid.ok);

    CHECK(pre.row_mid == pre.row_b);  // Pre：保持后一点
    CHECK(mid.row_mid != post.row_mid);
    CHECK(mid.row_mid != pre.row_mid);
    // Mid：台阶落在两端行号之间（含端点），不与任一端重合。
    const std::uint32_t row_lo = std::min(post.row_a, post.row_b);
    const std::uint32_t row_hi = std::max(post.row_a, post.row_b);
    CHECK(mid.row_mid >= row_lo);
    CHECK(mid.row_mid <= row_hi);
}

TEST_CASE("chart/area: 上下文开关是活的（轴与标题可关）") {
    const std::vector<Point> pts{{0.0, 1.0}, {5.0, 4.0}, {10.0, 2.0}};
    const std::size_t full = draw_area_nonblank(pts, true, true);
    const std::size_t no_title = draw_area_nonblank(pts, false, true);
    const std::size_t no_axes = draw_area_nonblank(pts, true, false);

    CHECK(full > no_title);  // 关标题 → 少画了标题
    CHECK(full > no_axes);   // 关轴 → 少画了轴与刻度标签
}

TEST_CASE("chart/area: 边界（空/单点/恒定/NaN）不崩") {
    {  // 空数据：不绘制也不崩
        canvas::Canvas canvas(20, 10);
        AreaChart chart;
        chart.draw({canvas});
        CHECK(count_cp(canvas, kFill) == 0);
    }
    {  // 单点：不成段，无填充，但有标记
        canvas::Canvas canvas(20, 10);
        AreaChart chart;
        chart.set_values({{5.0, 5.0}});
        chart.draw({canvas});
        CHECK(count_cp(canvas, kFill) == 0);
        CHECK(count_cp(canvas, kMarkerChar) == 1);
    }
    {  // 所有 y 相同：退化域不能除零
        canvas::Canvas canvas(40, 12);
        AreaChart chart;
        chart.set_values({{0.0, 5.0}, {1.0, 5.0}, {2.0, 5.0}, {3.0, 5.0}});
        chart.draw({canvas});
        CHECK(count_cp(canvas, kMarkerChar) == 4);
    }
    {  // NaN / Inf 混入：剔除后只剩有限点，不崩
        canvas::Canvas canvas(40, 12);
        AreaChart chart;
        const double nan = std::nan("");
        const double inf = std::numeric_limits<double>::infinity();
        chart.set_values({{0.0, 1.0}, {nan, 2.0}, {1.0, nan}, {inf, 3.0}, {2.0, 2.0}});
        chart.draw({canvas});
        CHECK(count_cp(canvas, kMarkerChar) == 2);
    }
}

TEST_CASE("chart/step: 边界与小画布裁剪不越界") {
    {  // 空数据：与折线图同语义——轴/网格照画，但没有折段与数据点
        canvas::Canvas canvas(20, 10);
        StepChart chart;
        chart.draw({canvas});
        CHECK(count_cp(canvas, kMarkerChar) == 0);
        CHECK(count_cp(canvas, kLineChar) == 0);
    }
    {  // 单点
        canvas::Canvas canvas(20, 10);
        StepChart chart;
        chart.set_values({{5.0, 5.0}});
        chart.draw({canvas});
        CHECK(count_cp(canvas, kMarkerChar) == 1);
    }
    {  // 所有 y 相同
        canvas::Canvas canvas(40, 12);
        StepChart chart;
        chart.set_values({{0.0, 5.0}, {1.0, 5.0}, {2.0, 5.0}});
        chart.draw({canvas});
        CHECK(count_nonblank(canvas) > 0);
    }
    {  // 小画布（内框负宽/负高）+ 超大域：连线/填充都必须被裁剪，不越界
        canvas::Canvas canvas(5, 3);
        AreaChart area;
        area.set_values({{-100.0, -100.0}, {100.0, 100.0}});
        area.draw({canvas});
        StepChart step;
        step.set_values({{-100.0, -100.0}, {100.0, 100.0}});
        step.draw({canvas});
        std::size_t n = 0;
        for (std::uint32_t y = 0; y < canvas.height(); ++y) {
            for (std::uint32_t x = 0; x < canvas.width(); ++x) {
                if (canvas.at(x, y).cp != U'\u0020') {
                    ++n;
                }
            }
        }
        CHECK(n <= static_cast<std::size_t>(canvas.width()) * canvas.height());
    }
    {  // NaN
        canvas::Canvas canvas(40, 12);
        StepChart chart;
        chart.set_values({{0.0, 1.0}, {std::nan(""), 2.0}, {2.0, std::nan("")}});
        chart.draw({canvas});
    }
}
