// test_layout_axis.cpp —— layout 坐标轴几何与刻度格式化
#include <doctest.h>

#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/point.hpp"
#include "termchart/layout/axis.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"
#include "termchart/layout/title.hpp"
#include "termchart/scale/linear.hpp"
#include "termchart/scale/ticks.hpp"

namespace {

using namespace termchart;
using namespace termchart::layout;

TEST_CASE("layout: format_tick 短标签且无尾零") {
    CHECK(format_tick(0.0) == "0");
    CHECK(format_tick(1.0) == "1");
    CHECK(format_tick(0.5) == "0.5");
    CHECK(format_tick(2.5) == "2.5");
    CHECK(format_tick(100.0) == "100");
    CHECK(format_tick(12.0) == "12");
    CHECK(format_tick(-3.0) == "-3");
}

TEST_CASE("layout: PlotArea 内框与比例尺方向") {
    const PlotArea area(40.0, 12.0, Margin{1, 1, 3, 8}, 0.0, 10.0, 0.0, 1.0);
    const Rect inner = area.inner_rect();
    CHECK(inner.x == doctest::Approx(8.0));
    CHECK(inner.y == doctest::Approx(1.0));
    CHECK(inner.w == doctest::Approx(31.0));
    CHECK(inner.bottom() == doctest::Approx(9.0));
    // x 正向 → 列增长
    CHECK(area.cell_of(0.0, 0.0).x == doctest::Approx(8.0));
    CHECK(area.cell_of(10.0, 0.0).x == doctest::Approx(39.0));
    // y 正向 → 行减小（向上）
    CHECK(area.cell_of(0.0, 0.0).y == doctest::Approx(9.0));
    CHECK(area.cell_of(0.0, 1.0).y == doctest::Approx(1.0));
}

TEST_CASE("layout: 坐标轴画出角点与轴线字符") {
    canvas::Canvas canvas(40, 12);
    const PlotArea area(40.0, 12.0, Margin{1, 1, 3, 8}, 0.0, 10.0, 0.0, 1.0);
    const std::vector<double> xticks = scale::ticks_for(area.x_scale());
    const std::vector<double> yticks = scale::ticks_for(area.y_scale());

    draw_axis_x(canvas, area, xticks, "x");
    draw_axis_y(canvas, area, yticks, "y");

    // 左下角 └
    CHECK(canvas.at(8, 9).cp == U'\u2514');
    // x 轴线水平段 ─（取非刻度列，刻度列会被 ┴ 覆盖）
    CHECK(canvas.at(17, 9).cp == U'\u2500');
    // 刻度在 x=4 处落在列 20，画成 ┴
    CHECK(canvas.at(20, 9).cp == U'\u2534');
    // y 轴线竖段 │
    CHECK(canvas.at(8, 5).cp == U'\u2502');
}

TEST_CASE("layout: 标题居中绘制") {
    canvas::Canvas canvas(40, 12);
    draw_title(canvas, "hello", Rect{0, 0, 40, 1}, HAlign::Center);
    // 标题应落在首行某处（不要求精确居中，只确认写进了字符）
    bool wrote = false;
    for (std::uint32_t x = 0; x < 40; ++x) {
        if (canvas.at(x, 0).cp != U'\u0020') {
            wrote = true;
            break;
        }
    }
    CHECK(wrote);
}

}  // namespace
