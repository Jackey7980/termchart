// test_chart_to_scene_batch.cpp —— 批量验证"剩余图种"的 to_scene 迁移
//
// 只验证三件事：
//   1) to_scene 返回 true 且产出的 Scene 非空；
//   2) 轴/网格/标题/图例是 TextOp（字符艺术忠实保留），且几何被 begin/end GroupOp 包住；
//   3) 图种自有几何是真矢量（RectOp/FillOp/StrokeOp），而非逐格字符转录。
#include <doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/scene_renderer.hpp"
#include "termchart/chart/cartesian/area.hpp"
#include "termchart/chart/cartesian/boxplot.hpp"
#include "termchart/chart/cartesian/candlestick.hpp"
#include "termchart/chart/cartesian/heatmap.hpp"
#include "termchart/chart/cartesian/hist.hpp"
#include "termchart/chart/cartesian/qq.hpp"
#include "termchart/chart/cartesian/scatter_chart.hpp"
#include "termchart/chart/scene_sink.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"

namespace {

using namespace termchart;
using namespace termchart::chart;

std::size_t count_kind(const scene::Scene& s, scene::CommandKind kind) {
    std::size_t n = 0;
    for (const auto& c : s.commands()) {
        if (scene::kind_of(c) == kind) {
            ++n;
        }
    }
    return n;
}

// 只数分组内部的命令（数据图元）；轴/网格在分组之外，避免把轴线也算进来。
std::size_t count_grouped(const scene::Scene& s, scene::CommandKind kind) {
    std::size_t n = 0;
    int depth = 0;
    for (const auto& c : s.commands()) {
        if (const auto* g = std::get_if<scene::GroupOp>(&c); g != nullptr) {
            depth += g->begin ? 1 : -1;
            continue;
        }
        if (depth > 0 && scene::kind_of(c) == kind) {
            ++n;
        }
    }
    return n;
}

}  // namespace

TEST_CASE("to_scene: 面积图产出真实矢量 Scene（填充+折线，且被 GroupOp 包住）") {
    AreaChart chart;
    chart.set_values({{0.0, 1.0}, {1.0, 3.0}, {2.0, 2.0}, {3.0, 5.0}}, Color::ansi16(2), "a");
    scene::Scene s;
    CHECK(chart.to_scene(s, 50, 14));
    CHECK_FALSE(s.commands().empty());
    CHECK(count_kind(s, scene::CommandKind::Group) >= 2);  // begin + end 成对
    CHECK(count_kind(s, scene::CommandKind::Text) > 0);     // 轴/标题是字符
    CHECK(count_grouped(s, scene::CommandKind::Rect) >= 1);   // 逐列填充是矢量矩形
    CHECK(count_grouped(s, scene::CommandKind::Stroke) >= 1); // 折线是矢量描边
}

TEST_CASE("to_scene: K 线图产出真实矢量 Scene（实体/影线被 GroupOp 包住）") {
    CandlestickChart chart;
    chart.set_data({
        CandlestickChart::Ohlc{10.0, 12.0, 9.0, 11.0},
        CandlestickChart::Ohlc{11.0, 15.0, 10.5, 14.0},
        CandlestickChart::Ohlc{14.0, 14.5, 12.0, 12.5},
    });
    scene::Scene s;
    CHECK(chart.to_scene(s, 50, 14));
    CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    CHECK(count_grouped(s, scene::CommandKind::Rect) >= 1);   // 实体矩形
    CHECK(count_grouped(s, scene::CommandKind::Stroke) >= 1); // 影线
}

TEST_CASE("to_scene: 箱线图产出真实矢量 Scene（箱体/须/中位被 GroupOp 包住）") {
    BoxPlotChart chart;
    chart.set_data({
        BoxPlotChart::BoxStats{.lo = 1.0, .q1 = 2.0, .med = 3.0, .q3 = 4.0, .hi = 5.0, .outliers = {}},
        BoxPlotChart::BoxStats{.lo = 2.0, .q1 = 3.0, .med = 3.5, .q3 = 4.5, .hi = 6.0,
                               .outliers = std::vector<double>{7.5}},
    });
    scene::Scene s;
    CHECK(chart.to_scene(s, 50, 14));
    CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    CHECK(count_grouped(s, scene::CommandKind::Rect) >= 1);   // 箱体/端帽
    CHECK(count_grouped(s, scene::CommandKind::Stroke) >= 1); // 须
}

TEST_CASE("to_scene: 热力图产出真实矢量 Scene（色块是矢量矩形，非 █ 逐格）") {
    HeatmapChart chart;
    chart.set_values({{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}, {0.5, 0.5}},
                     color::ColormapKind::Viridis, "h");
    scene::Scene s;
    CHECK(chart.to_scene(s, 50, 14));
    CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    CHECK(count_grouped(s, scene::CommandKind::Rect) >= 1);  // 色块是矢量矩形
}

TEST_CASE("to_scene: 直方图产出真实矢量 Scene（柱是实心矩形，非 █ 逐格）") {
    HistChart chart;
    chart.set_values({1.0, 1.5, 2.0, 2.0, 3.0, 3.5, 4.0, 4.5}, Color::ansi16(2), "h");
    scene::Scene s;
    CHECK(chart.to_scene(s, 50, 14));
    CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    CHECK(count_grouped(s, scene::CommandKind::Rect) >= 1);  // 柱是矢量矩形
}

TEST_CASE("to_scene: 散点图产出真实 Scene（点标记落在 GroupOp 内）") {
    ScatterChart chart;
    Series a;
    a.name = "a";
    a.color = Color::ansi16(2);
    a.points = {{0.0, 0.0}, {1.0, 1.0}, {2.0, 4.0}};
    chart.add_series(std::move(a));
    scene::Scene s;
    CHECK(chart.to_scene(s, 50, 14));
    CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    CHECK(count_grouped(s, scene::CommandKind::Text) >= 1);  // 点标记是字符字形
}

TEST_CASE("to_scene: QQ 图产出真实矢量 Scene（参考线是矢量描边）") {
    QQChart chart;
    chart.set_sample({0.1, -0.5, 0.3, 1.2, -1.1, 0.7, 0.05, -0.3, 0.9, -0.8});
    scene::Scene s;
    CHECK(chart.to_scene(s, 50, 14));
    CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    CHECK(count_grouped(s, scene::CommandKind::Stroke) >= 1); // 参考线
    CHECK(count_grouped(s, scene::CommandKind::Text) >= 1);    // 样本点标记
}

TEST_CASE("to_scene: 面积图的 Scene 能被 render_scene 落到字符格（柱/填充仍实心）") {
    AreaChart chart;
    chart.set_values({{0.0, 1.0}, {1.0, 3.0}, {2.0, 2.0}, {3.0, 5.0}}, Color::ansi16(2), "a");
    scene::Scene s;
    CHECK(chart.to_scene(s, 50, 14));

    canvas::Canvas canvas(50, 14);
    canvas::render_scene(s, canvas);

    // 轴线仍是细 ─，未被盲文光栅成实心带。
    std::size_t dashes = 0;
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            if (canvas.at(x, y).cp == U'\u2500') {
                ++dashes;
            }
        }
    }
    CHECK(dashes >= 10);
}
