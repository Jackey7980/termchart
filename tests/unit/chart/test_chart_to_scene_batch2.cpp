// test_chart_to_scene_batch2.cpp —— 第二批 to_scene 迁移冒烟测试（cartesian 剩余 14 + financial/yield_curve）
//
// 与 test_chart_to_scene_batch.cpp 同款断言：每个图种 to_scene 返回 true、产出非空
// Scene、几何被 begin/end GroupOp 包住、轴/网格是 TextOp（字符艺术保留）。
#include <doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/scene_renderer.hpp"
#include "termchart/chart/cartesian/bubble.hpp"
#include "termchart/chart/cartesian/bullet.hpp"
#include "termchart/chart/cartesian/contour.hpp"
#include "termchart/chart/cartesian/dumbbell.hpp"
#include "termchart/chart/cartesian/errorbar.hpp"
#include "termchart/chart/cartesian/gantt.hpp"
#include "termchart/chart/cartesian/mosaic.hpp"
#include "termchart/chart/cartesian/parallel.hpp"
#include "termchart/chart/cartesian/ridgeline.hpp"
#include "termchart/chart/cartesian/sparkline.hpp"
#include "termchart/chart/cartesian/step.hpp"
#include "termchart/chart/cartesian/streamgraph.hpp"
#include "termchart/chart/cartesian/violin.hpp"
#include "termchart/chart/cartesian/waterfall.hpp"
#include "termchart/chart/financial/yield_curve.hpp"
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

}  // namespace

TEST_CASE("to_scene batch2: 15 图种均产出 Group 包住的矢量 Scene") {
    // bubble
    {
        BubbleChart chart;
        chart.set_data({BubbleChart::Bubble{Point{0.0, 0.0}, 1.0},
                         BubbleChart::Bubble{Point{1.0, 1.0}, 2.0}});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK_FALSE(s.commands().empty());
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
        CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    }
    // bullet
    {
        BulletChart chart;
        chart.set_rows({BulletChart::Row{.label = "a",
                                         .value = 3.0,
                                         .target = 4.0,
                                         .bands = {1.0, 2.0, 3.0}}});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
        CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    }
    // contour
    {
        ContourChart chart;
        chart.set_field({{0.0, 1.0, 2.0}, {1.0, 2.0, 3.0}, {2.0, 3.0, 4.0}});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
        CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    }
    // dumbbell
    {
        DumbbellChart chart;
        chart.set_rows({DumbbellChart::Row{.label = "a", .a = 1.0, .b = 3.0}});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
        CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    }
    // errorbar
    {
        ErrorBarChart chart;
        chart.set_data({ErrorBarChart::ErrorPoint{
            .x = 1.0, .y = 2.0, .y_lo = 1.5, .y_hi = 2.5}});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
        CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    }
    // gantt
    {
        GanttChart chart;
        chart.set_data({GanttChart::Task{.name = "t", .start = 0.0, .end = 5.0}});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
        CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    }
    // mosaic
    {
        MosaicChart chart;
        chart.set_counts({{1.0, 2.0}, {3.0, 4.0}}, {"r0", "r1"}, {"c0", "c1"});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
        CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    }
    // parallel
    {
        ParallelChart chart;
        chart.set_axes({"a", "b"}, {{0.0, 1.0}, {1.0, 2.0}});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
        CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    }
    // ridgeline
    {
        RidgelineChart chart;
        RidgelineChart::Group g;
        g.name = "g";
        g.color = Color::ansi16(2);
        g.values = {1.0, 2.0, 3.0};
        chart.set_groups({g});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
        CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    }
    // sparkline
    {
        SparklineChart chart;
        chart.set_values({1.0, 2.0, 3.0, 2.0, 1.0});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
        CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    }
    // step
    {
        StepChart chart;
        chart.set_values({Point{0.0, 0.0}, Point{1.0, 1.0}, Point{2.0, 0.0}});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
        CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    }
    // streamgraph
    {
        StreamgraphChart chart;
        Series layer;
        layer.name = "l";
        layer.color = Color::ansi16(2);
        layer.points = {{0.0, 0.0}, {1.0, 1.0}, {2.0, 2.0}};
        chart.set_series({layer});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
        CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    }
    // violin
    {
        ViolinChart chart;
        chart.set_data({{1.0, 2.0, 3.0}, {2.0, 3.0, 4.0}});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
        CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    }
    // waterfall
    {
        WaterfallChart chart;
        chart.set_data({WaterfallChart::Item{.label = "a", .value = 1.0},
                         WaterfallChart::Item{.label = "b", .value = 2.0, .is_total = true}});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
        CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    }
    // yield_curve
    {
        YieldCurveChart chart;
        chart.set_curve({1.0, 2.0, 3.0}, {0.01, 0.02, 0.03}, Color::ansi16(2), "yc");
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
        CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    }
}

TEST_CASE("to_scene batch2: violin 的 Scene 能经 render_scene 落到字符格") {
    ViolinChart chart;
    chart.set_data({{1.0, 2.0, 3.0, 2.5, 1.5}, {2.0, 3.0, 4.0, 3.5, 2.5}});
    scene::Scene s;
    CHECK(chart.to_scene(s, 50, 14));

    canvas::Canvas canvas(50, 14);
    canvas::render_scene(s, canvas);
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
