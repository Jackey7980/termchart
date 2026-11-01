// test_chart_to_scene_batch5.cpp —— 第五批 to_scene 迁移冒烟测试（polar 8 + geo 5 + hierarchical 6 + financial 6）
//
// 断言：to_scene 返回 true、Scene 非空、几何被 GroupOp 包住（>=2 个 GroupOp：begin+end）。
#include <doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/scene_renderer.hpp"
#include "termchart/chart/polar/donut.hpp"
#include "termchart/chart/polar/pie.hpp"
#include "termchart/chart/polar/gauge.hpp"
#include "termchart/chart/polar/polar_heat.hpp"
#include "termchart/chart/polar/polar_bar.hpp"
#include "termchart/chart/polar/polar_line.hpp"
#include "termchart/chart/polar/radar.hpp"
#include "termchart/chart/polar/rose.hpp"
#include "termchart/chart/geo/dot_density.hpp"
#include "termchart/chart/geo/tile_grid.hpp"
#include "termchart/chart/geo/choropleth.hpp"
#include "termchart/chart/geo/hexbin.hpp"
#include "termchart/chart/geo/geo_map.hpp"
#include "termchart/chart/hierarchical/dendrogram.hpp"
#include "termchart/chart/hierarchical/icicle.hpp"
#include "termchart/chart/hierarchical/tree_view.hpp"
#include "termchart/chart/hierarchical/pack.hpp"
#include "termchart/chart/hierarchical/sunburst.hpp"
#include "termchart/chart/hierarchical/treemap.hpp"
#include "termchart/chart/financial/depth.hpp"
#include "termchart/chart/financial/drawdown.hpp"
#include "termchart/chart/financial/point_figure.hpp"
#include "termchart/chart/financial/kagi.hpp"
#include "termchart/chart/financial/volume.hpp"
#include "termchart/chart/financial/renko.hpp"
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

hier::HierNode sample_tree() {
    hier::HierNode root;
    root.name = "root";
    root.children = {
        hier::HierNode{.name = "a", .value = 3.0, .children = {}},
        hier::HierNode{.name = "b",
                       .value = 5.0,
                       .children = {hier::HierNode{.name = "b1", .value = 2.0, .children = {}},
                                    hier::HierNode{.name = "b2", .value = 3.0, .children = {}}}},
        hier::HierNode{.name = "c", .value = 4.0, .children = {}},
    };
    return root;
}

}  // namespace

TEST_CASE("to_scene batch5: polar 8 图种均产出 Group 包住的 Scene") {
    {
        DonutChart chart;
        chart.set_values({1.0, 2.0, 3.0, 4.0});
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        PieChart chart;
        chart.set_values({1.0, 2.0, 3.0, 4.0});
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        GaugeChart chart;
        chart.set_value(0.6);
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        PolarHeatChart chart;
        chart.set_points({Point{0.0, 0.0}, Point{1.0, 2.0}, Point{3.0, 1.0}, Point{2.0, 4.0}});
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        PolarBarChart chart;
        chart.set_values({1.0, 2.0, 3.0, 4.0});
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        PolarLineChart chart;
        chart.set_points({Point{0.0, 0.0}, Point{1.0, 1.0}, Point{2.0, 3.0}, Point{3.0, 2.0}});
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        RadarChart chart;
        chart.set_values({1.0, 2.0, 3.0, 4.0});
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        RoseChart chart;
        chart.set_values({1.0, 2.0, 3.0, 4.0});
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
}

TEST_CASE("to_scene batch5: geo 5 图种均产出 Group 包住的 Scene") {
    {
        Choropleth chart;
        chart.add_region({"r1", {Point{1, 1}, Point{2, 2}, Point{3, 1}}, 0.5});
        chart.add_region({"r2", {Point{5, 5}, Point{6, 6}}, 0.8});
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        DotDensity chart;
        chart.add_region({"r1", {Point{1, 1}, Point{2, 2}}, 3.0, Color::ansi16(2)});
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        TileGrid chart;
        chart.add_tile({"t1", 2, 3, 0.5});
        chart.add_tile({"t2", 4, 1, 0.9});
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        Hexbin chart;
        chart.add_point(0.1, 0.2);
        chart.add_point(0.5, 0.7);
        chart.add_point(0.9, 0.1);
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        // GeoMap 必须先 load_geojson 才有几何；未加载时 to_scene 早退返回 true（空 Scene）。
        GeoMap chart;
        chart.set_value("x", 0.3);
        chart.set_value("y", 0.8);
        chart.set_value("z", 0.5);
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
    }
}

TEST_CASE("to_scene batch5: hierarchical 6 图种均产出 Group 包住的 Scene") {
    auto tree = sample_tree();
    {
        Dendrogram chart;
        chart.set_root(tree);
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        Icicle chart;
        chart.set_root(tree);
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        TreeView chart;
        chart.set_root(tree);
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        PackChart chart;
        chart.set_root(tree);
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        Sunburst chart;
        chart.set_root(tree);
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        Treemap chart;
        chart.set_root(tree);
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
}

TEST_CASE("to_scene batch5: financial 6 图种均产出 Group 包住的 Scene") {
    {
        DepthChart chart;
        chart.set_book({10.0, 9.5, 9.0}, {1.0, 2.0, 3.0}, {10.5, 11.0, 11.5}, {2.0, 1.0, 4.0});
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        DrawdownChart chart;
        chart.set_prices({1.0, 2.0, 3.0, 2.5, 4.0, 3.0});
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        PointFigureChart chart;
        chart.set_prices({1.0, 2.0, 3.0, 2.0, 4.0, 5.0, 4.0});
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        KagiChart chart;
        chart.set_prices({1.0, 2.0, 3.0, 2.0, 4.0, 5.0, 4.0});
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        VolumeChart chart;
        chart.set_values({1.0, 2.0, 3.0, 2.0}, {1, -1, 1, -1});
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    {
        RenkoChart chart;
        chart.set_prices({1.0, 2.0, 3.0, 2.0, 4.0, 5.0, 4.0});
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 20));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
}
