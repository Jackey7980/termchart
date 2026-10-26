// test_chart_to_scene_batch3.cpp —— 第三批 to_scene 迁移冒烟测试（indicator 5 + text 4）
//
// 这些图种多为仪表盘/表格/词云，不用 Area2D，几何以 SceneSink 的 put/text（TextOp）
// 复刻，整体包在 begin_group/end_group(0) 中。断言：to_scene 返回 true、Scene 非空、
// 几何被 GroupOp 包住（>=2 个 GroupOp：begin+end）。
#include <doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/scene_renderer.hpp"
#include "termchart/chart/indicator/gauge_kpi.hpp"
#include "termchart/chart/indicator/progress_ring.hpp"
#include "termchart/chart/indicator/star_rating.hpp"
#include "termchart/chart/indicator/status_light.hpp"
#include "termchart/chart/indicator/thermometer.hpp"
#include "termchart/chart/text/progress.hpp"
#include "termchart/chart/text/table.hpp"
#include "termchart/chart/text/tag_cloud.hpp"
#include "termchart/chart/text/wordcloud.hpp"
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

TEST_CASE("to_scene batch3: indicator 5 图种均产出 Group 包住的 Scene") {
    // gauge_kpi
    {
        GaugeKpi chart;
        chart.set_label("CPU");
        chart.set_value("42%");
        chart.set_delta("+3%", true);
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 12));
        CHECK_FALSE(s.commands().empty());
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // progress_ring
    {
        ProgressRing chart;
        chart.set_fraction(0.5);
        chart.set_color(Color::ansi16(2));
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 12));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // star_rating
    {
        StarRating chart;
        chart.set_rating(3.5);
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 12));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // status_light
    {
        StatusLight chart;
        chart.set_label("OK");
        chart.set_state(LightState::Ok);
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 12));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // thermometer
    {
        Thermometer chart;
        chart.set_value(0.6);
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 12));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
}

TEST_CASE("to_scene batch3: text 4 图种均产出 Group 包住的 Scene") {
    // progress
    {
        Progress chart;
        chart.add_bar("a", 0.5, Color::ansi16(2));
        chart.add_bar("b", 0.8, Color::ansi16(4));
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 12));
        CHECK_FALSE(s.commands().empty());
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // table
    {
        Table chart;
        chart.add_column("Name");
        chart.add_column("Val");
        chart.add_row({"A", "1"});
        chart.add_row({"B", "2"});
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 12));
        CHECK_FALSE(s.commands().empty());
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // tag_cloud
    {
        TagCloud chart;
        chart.add_tag("foo", 2.0);
        chart.add_tag("bar", 1.0);
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 12));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
    // wordcloud
    {
        Wordcloud chart;
        chart.add_word("termchart", 5.0);
        chart.add_word("chart", 3.0);
        scene::Scene s;
        CHECK(chart.to_scene(s, 40, 12));
        CHECK(count_kind(s, scene::CommandKind::Group) >= 2);
    }
}

TEST_CASE("to_scene batch3: text::progress 的 Scene 能经 render_scene 落到字符格") {
    Progress chart;
    chart.add_bar("a", 0.7, Color::ansi16(2));
    chart.add_bar("b", 0.4, Color::ansi16(4));
    scene::Scene s;
    CHECK(chart.to_scene(s, 40, 12));

    canvas::Canvas canvas(40, 12);
    canvas::render_scene(s, canvas);
    std::size_t filled = 0;
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            if (canvas.at(x, y).cp != U' ') {
                ++filled;
            }
        }
    }
    CHECK(filled > 0);
}
