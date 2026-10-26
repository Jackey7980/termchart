// test_chart_to_scene_batch4.cpp —— 第四批 to_scene 迁移冒烟测试（statistical 11 + flow 2）
//
// 断言：to_scene 返回 true、Scene 非空、几何被 GroupOp 包住（>=2 个 GroupOp：begin+end）。
#include <doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/scene_renderer.hpp"
#include "termchart/chart/statistical/autocorr.hpp"
#include "termchart/chart/statistical/bland_altman.hpp"
#include "termchart/chart/statistical/corr.hpp"
#include "termchart/chart/statistical/distribution.hpp"
#include "termchart/chart/statistical/ecdf.hpp"
#include "termchart/chart/statistical/forest.hpp"
#include "termchart/chart/statistical/pca.hpp"
#include "termchart/chart/statistical/regression.hpp"
#include "termchart/chart/statistical/roc.hpp"
#include "termchart/chart/statistical/seasonal.hpp"
#include "termchart/chart/statistical/survival.hpp"
#include "termchart/chart/flow/funnel.hpp"
#include "termchart/chart/flow/stream.hpp"
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

TEST_CASE("to_scene batch4: statistical 11 图种均产出 Group 包住的 Scene") {
    // autocorr
    {
        AutocorrChart chart;
        chart.set_values({0.5, -0.2, 0.1, 0.3, -0.4, 0.2});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // bland_altman
    {
        BlandAltmanChart chart;
        chart.set_pairs({1.0, 2.0, 3.0}, {1.1, 2.1, 2.9});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // corr
    {
        CorrMatrixChart chart;
        chart.set_matrix({"a", "b", "c"}, {{1.0, 0.2, -0.1}, {0.2, 1.0, 0.5}, {-0.1, 0.5, 1.0}});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // distribution
    {
        DistributionChart chart;
        chart.set_values({1.0, 2.0, 1.5, 2.5, 3.0, 1.2, 2.8});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // ecdf
    {
        EcdfChart chart;
        chart.set_values({1.0, 2.0, 3.0, 4.0, 5.0});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // forest
    {
        ForestChart chart;
        chart.set_studies({{"s1", 1.2, 0.8, 1.6}, {"s2", 0.9, 0.5, 1.3}});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // pca
    {
        PcaChart chart;
        chart.set_data({{1.0, 2.0}, {2.0, 1.0}, {1.5, 1.5}, {0.5, 1.0}});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // regression
    {
        RegressionChart chart;
        chart.set_points({Point{0.0, 0.0}, Point{1.0, 1.0}, Point{2.0, 2.1}, Point{3.0, 3.2}});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // roc
    {
        RocChart chart;
        chart.set_scores({0.9, 0.8, 0.6, 0.4, 0.2}, {1, 1, 0, 0, 0});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // seasonal
    {
        SeasonalChart chart;
        chart.set_values({1.0, 2.0, 3.0, 2.0, 1.0, 2.0, 3.0, 2.0, 1.0, 2.0, 3.0, 2.0,
                          2.0, 3.0, 4.0, 3.0, 2.0, 3.0, 4.0, 3.0, 2.0, 3.0, 4.0, 3.0});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // survival
    {
        SurvivalChart chart;
        chart.set_data({1.0, 2.0, 3.0, 4.0, 5.0}, {1, 1, 0, 1, 0});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
}

TEST_CASE("to_scene batch4: flow 2 图种均产出 Group 包住的 Scene") {
    // funnel
    {
        FunnelChart chart;
        chart.set_stages({{"a", 100.0}, {"b", 60.0}, {"c", 30.0}});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // stream
    {
        StreamChart chart;
        Series s1;
        s1.name = "s1";
        s1.color = Color::ansi16(2);
        s1.points = {Point{0.0, 1.0}, Point{1.0, 2.0}, Point{2.0, 1.5}};
        Series s2;
        s2.name = "s2";
        s2.color = Color::ansi16(4);
        s2.points = {Point{0.0, 2.0}, Point{1.0, 1.0}, Point{2.0, 2.5}};
        chart.set_series({s1, s2});
        scene::Scene s;
        CHECK(chart.to_scene(s, 50, 14));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
}

TEST_CASE("to_scene batch4: regression 的 Scene 能经 render_scene 落到字符格") {
    RegressionChart chart;
    chart.set_points({Point{0.0, 0.0}, Point{1.0, 1.0}, Point{2.0, 2.1}, Point{3.0, 3.2}});
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
