// test_chart_hist_heatmap.cpp —— 直方图 / 热力图 端到端绘制
//
// 断言重点是"语义"而非"没崩"：
//   * 直方图：分箱规则确定、频数守恒、最大柱触顶、改箱数边界随之变、空输入/NaN 不崩；
//   * 热力图：两簇点落在不同格且颜色不同、点数越多色值越"热"（有序，不只是不等）、
//             draw_title/draw_axes 是活的、NaN/Inf 不污染定义域。
#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/heatmap.hpp"
#include "termchart/chart/cartesian/hist.hpp"
#include "termchart/chart/context.hpp"
#include "termchart/color/colormap.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rgb.hpp"
#include "termchart/core/style.hpp"
#include "termchart/layout/box.hpp"

namespace {

using namespace termchart;
using namespace termchart::chart;

std::size_t count_nonblank(const canvas::Canvas& c) {
    std::size_t n = 0;
    for (std::uint32_t y = 0; y < c.height(); ++y) {
        for (std::uint32_t x = 0; x < c.width(); ++x) {
            const char32_t cp = c.at(x, y).cp;
            if (cp != U'\u0020' && cp != U'\u0000') {  // 空格与宽字符续接格都不算
                ++n;
            }
        }
    }
    return n;
}

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

bool has_char(const canvas::Canvas& c, char32_t cp) {
    return count_cp(c, cp) > 0;
}

std::vector<double> ramp_samples() {
    std::vector<double> s;
    for (int i = 0; i < 100; ++i) {
        s.push_back(static_cast<double>(i % 20));  // 0..19 各 5 个
    }
    return s;
}

// 用给定装饰开关跑一次直方图，返回非空格计数（用于上下文开关断言）。
std::size_t draw_hist_nonblank(const std::vector<double>& samples, bool decor) {
    canvas::Canvas canvas(50, 14);
    HistChart chart;
    chart.set_values(samples);
    chart.set_title("HH");
    DrawContext ctx{canvas};
    ctx.draw_axes = decor;
    ctx.draw_grid = decor;
    ctx.draw_legend = decor;
    chart.draw(ctx);
    return count_nonblank(canvas);
}

}  // namespace

TEST_CASE("chart/hist: Sturges 分箱、频数守恒、最大柱触顶") {
    const std::vector<double> samples = ramp_samples();  // n=100

    canvas::Canvas canvas(60, 20);
    HistChart chart;
    chart.set_values(samples, Color::ansi16(4), "h");
    chart.set_title("H");

    // Sturges：ceil(log2 100) + 1 = 7 + 1 = 8。
    CHECK(chart.bin_count() == 8);
    REQUIRE(chart.bin_edges().size() == 9);
    REQUIRE(chart.bin_counts().size() == 8);

    std::size_t sum = 0;
    std::size_t max_count = 0;
    for (std::size_t c : chart.bin_counts()) {
        sum += c;
        max_count = std::max(max_count, c);
    }
    CHECK(sum == samples.size());  // 频数之和 == 被纳入的样本数
    CHECK(max_count > 0);

    chart.draw({canvas});

    // 至少一根柱触顶：最高频数映射到的行应真有柱格（y 域精确取 [0, max_count]）。
    const layout::PlotArea& area = chart.last_area();
    const auto top_row = static_cast<std::uint32_t>(
        std::floor(area.y_scale().forward(static_cast<double>(max_count))));
    bool touches_top = false;
    for (std::uint32_t x = 0; x < canvas.width() && !touches_top; ++x) {
        if (canvas.at(x, top_row).cp == kBarChar) {
            touches_top = true;
        }
    }
    CHECK(touches_top);
    CHECK(count_cp(canvas, kBarChar) > 0);
}

TEST_CASE("chart/hist: set_bins 覆盖自动分箱（含 clamp 与恢复自动）") {
    HistChart chart;
    chart.set_values(ramp_samples());
    const std::size_t auto_bins = chart.bin_count();
    CHECK(auto_bins == 8);

    chart.set_bins(5);
    CHECK(chart.bin_count() == 5);
    CHECK(chart.bin_edges().size() == 6);
    CHECK(chart.bin_counts().size() == 5);

    chart.set_bins(128);  // 超过上限 → clamp 到 64
    CHECK(chart.bin_count() == 64);

    chart.set_bins(1);  // 下限
    CHECK(chart.bin_count() == 1);

    chart.set_bins(0);  // <=0 → 恢复自动
    CHECK(chart.bin_count() == auto_bins);
    chart.set_bins(-3);
    CHECK(chart.bin_count() == auto_bins);
}

TEST_CASE("chart/hist: 频数守恒在剔除 NaN/Inf 后仍成立、定义域有限") {
    canvas::Canvas canvas(50, 14);
    HistChart chart;
    const double nan = std::nan("");
    const double inf = std::numeric_limits<double>::infinity();
    chart.set_values({1.0, nan, 3.0, inf, 2.0, -inf, 2.5});
    chart.draw({canvas});

    std::size_t sum = 0;
    for (std::size_t c : chart.bin_counts()) {
        sum += c;
    }
    CHECK(sum == 4);  // 有限样本只有 1.0 / 3.0 / 2.0 / 2.5 四个，NaN 与 ±Inf 已剔除

    const layout::PlotArea& area = chart.last_area();
    CHECK(std::isfinite(area.x_scale().domain_min()));
    CHECK(std::isfinite(area.x_scale().domain_max()));
    CHECK(std::isfinite(area.y_scale().domain_min()));
    CHECK(std::isfinite(area.y_scale().domain_max()));
    // x 域取箱边界，落在有限样本区间内，不因 Inf 被撑开。
    CHECK(area.x_scale().domain_min() >= 1.0 - 1e-9);
    CHECK(area.x_scale().domain_max() <= 3.0 + 1e-9);
}

TEST_CASE("chart/hist: 空输入 / 全非有限 / 全同值 不崩且不除零") {
    {  // 未 set_values：不绘制也不崩
        canvas::Canvas canvas(20, 10);
        HistChart chart;
        chart.draw({canvas});
        CHECK(chart.bin_count() == 0);
        CHECK(count_nonblank(canvas) == 0);
        CHECK(std::isfinite(chart.last_area().y_scale().domain_min()));
        CHECK(std::isfinite(chart.last_area().y_scale().domain_max()));
    }
    {  // 全非有限：全部剔除，退化为空
        canvas::Canvas canvas(40, 12);
        HistChart chart;
        const double inf = std::numeric_limits<double>::infinity();
        chart.set_values({std::nan(""), inf, -inf});
        CHECK(chart.bin_count() == 0);
        chart.draw({canvas});
        CHECK(count_nonblank(canvas) == 0);
    }
    {  // 全同值：给单位宽度，不除零
        canvas::Canvas canvas(30, 10);
        HistChart chart;
        chart.set_values({5.0, 5.0, 5.0, 5.0});
        chart.draw({canvas});
        CHECK(chart.bin_count() >= 1);
        std::size_t sum = 0;
        for (std::size_t c : chart.bin_counts()) {
            sum += c;
        }
        CHECK(sum == 4);
    }
}

TEST_CASE("chart/hist: 上下文开关是活的（轴/网格/图例可关）") {
    const std::vector<double> samples = ramp_samples();
    const std::size_t full = draw_hist_nonblank(samples, true);
    const std::size_t bare = draw_hist_nonblank(samples, false);
    CHECK(bare > 0);    // 数据柱仍在
    CHECK(full > bare);  // 关掉装饰后确实少了轴/刻度/图例
}

TEST_CASE("chart/heatmap: 两簇点颜色不同，点数多的更热（色值有序）") {
    std::vector<Point> pts;
    for (int i = 0; i < 40; ++i) {  // 热簇：40 点，落在左下角
        const double v = 0.02 * static_cast<double>(i);
        pts.push_back(Point{v, v});
    }
    for (int i = 0; i < 8; ++i) {  // 冷簇：8 点，落在右上角（与热簇相隔很远）
        const double v = 9.0 + 0.1 * static_cast<double>(i);
        pts.push_back(Point{v, v});
    }

    canvas::Canvas canvas(60, 20);
    HeatmapChart chart;
    chart.set_values(pts);
    DrawContext ctx{canvas};
    ctx.draw_axes = false;
    ctx.draw_grid = false;
    ctx.draw_legend = false;
    chart.draw(ctx);

    CHECK(count_nonblank(canvas) > 0);

    const layout::PlotArea& area = chart.last_area();
    auto cell_fg = [&](double dx, double dy) {
        const Point p = area.cell_of(dx, dy);
        const auto px = static_cast<std::uint32_t>(std::floor(p.x));
        const auto py = static_cast<std::uint32_t>(std::floor(p.y));
        return canvas.at(px, py).style.fg();
    };
    const Color hot = cell_fg(0.4, 0.4);
    const Color cold = cell_fg(9.4, 9.4);

    CHECK(hot.kind() == ColorKind::Rgb);   // 真上色，不是单一默认色
    CHECK(cold.kind() == ColorKind::Rgb);
    CHECK(hot != cold);
    // viridis 上 t 越大越亮：点数多的格 luma 必须严格更大（有序，不只是不等）。
    CHECK(hot.rgb().luma() > cold.rgb().luma());
}

TEST_CASE("chart/heatmap: draw_title / draw_axes 是活的") {
    std::vector<Point> pts;
    for (int i = 0; i < 20; ++i) {
        pts.push_back(Point{0.1 * static_cast<double>(i), 0.1 * static_cast<double>(i)});
    }

    canvas::Canvas with_all(50, 16);
    HeatmapChart a;
    a.set_values(pts);
    a.set_title("HEAT");
    a.draw(DrawContext{with_all});
    CHECK(has_char(with_all, U'H'));  // 标题
    CHECK(has_char(with_all, U'y'));  // y 轴名
    const std::size_t full = count_nonblank(with_all);

    canvas::Canvas no_decor(50, 16);
    HeatmapChart b;
    b.set_values(pts);
    b.set_title("HEAT");
    DrawContext ctx{no_decor};
    ctx.draw_title = false;
    ctx.draw_axes = false;
    b.draw(ctx);
    CHECK_FALSE(has_char(no_decor, U'H'));
    CHECK_FALSE(has_char(no_decor, U'y'));
    CHECK(count_nonblank(no_decor) > 0);  // 数据格还在
    CHECK(count_nonblank(no_decor) < full);
}

TEST_CASE("chart/heatmap: 边界（NaN/全同/单点/小画布）不崩且定义域有限") {
    {  // NaN/Inf 混入：定义域不被污染
        canvas::Canvas canvas(50, 16);
        HeatmapChart chart;
        const double nan = std::nan("");
        const double inf = std::numeric_limits<double>::infinity();
        chart.set_values({{0.0, 0.0},   {1.0, 1.0},  {nan, 2.0},
                          {2.0, nan},   {inf, 3.0},  {3.0, -inf}});
        chart.draw({canvas});
        const layout::PlotArea& area = chart.last_area();
        CHECK(std::isfinite(area.x_scale().domain_min()));
        CHECK(std::isfinite(area.x_scale().domain_max()));
        CHECK(std::isfinite(area.y_scale().domain_min()));
        CHECK(std::isfinite(area.y_scale().domain_max()));
        CHECK(count_nonblank(canvas) > 0);
    }
    {  // 空输入：不崩，定义域仍是有限占位
        canvas::Canvas canvas(20, 10);
        HeatmapChart chart;
        chart.draw({canvas});
        CHECK(std::isfinite(chart.last_area().x_scale().domain_min()));
        CHECK(std::isfinite(chart.last_area().y_scale().domain_max()));
    }
    {  // 全部同值：给单位宽高，避免除零
        canvas::Canvas canvas(30, 12);
        HeatmapChart chart;
        chart.set_values({{5.0, 5.0}, {5.0, 5.0}, {5.0, 5.0}});
        chart.draw({canvas});
        CHECK(count_nonblank(canvas) > 0);
        CHECK(std::isfinite(chart.last_area().x_scale().domain_max()));
    }
    {  // 单点
        canvas::Canvas canvas(30, 12);
        HeatmapChart chart;
        chart.set_values({{7.0, 3.0}});
        chart.draw({canvas});
        CHECK(count_nonblank(canvas) > 0);
    }
    {  // 小画布 + 大量点：越界裁剪，写入量不超过画布格数
        canvas::Canvas canvas(3, 2);
        HeatmapChart chart;
        std::vector<Point> pts;
        for (int i = 0; i < 500; ++i) {
            pts.push_back(Point{0.1 * static_cast<double>(i),
                                static_cast<double>(i % 7) * 0.3});
        }
        chart.set_values(pts);
        chart.draw({canvas});
        CHECK(count_nonblank(canvas) <=
              static_cast<std::size_t>(canvas.width()) * canvas.height());
    }
}
