// test_chart_scene.cpp —— 图表走**真实 Scene** 通路
//
// 这里验证的不是"能不能画出图"，而是"画出来的东西真的是 Scene"：
//   * 折线是 MoveTo/LineTo + StrokeOp（矢量），不是一串逐格字符；
//   * 每条系列被 GroupOp 包住，且 begin/end 成对、id 与外层不串；
//   * 轴/标题/图例是 TextOp（它们本来就是字符艺术，忠实保留）；
//   * 同一份 Scene 交给 render_scene 能落到 Canvas，轴角仍在。
#include <doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/api/chart.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/scene_renderer.hpp"
#include "termchart/chart/cartesian/bar_chart.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/chart/scene_sink.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"

namespace {

using namespace termchart;
using namespace termchart::chart;

chart::LineChart make_chart() {
    LineChart chart;
    chart.set_title("scene");
    chart::Series a;
    a.name = "a";
    a.color = Color::ansi16(2);
    a.points = {{0.0, 0.0}, {1.0, 1.0}, {2.0, 4.0}};
    chart::Series b;
    b.name = "b";
    b.color = Color::ansi16(3);
    b.points = {{0.0, 3.0}, {1.0, 2.0}, {2.0, 1.0}};
    chart.add_series(std::move(a));
    chart.add_series(std::move(b));
    return chart;
}

std::size_t count_kind(const scene::Scene& s, scene::CommandKind kind) {
    std::size_t n = 0;
    for (const auto& c : s.commands()) {
        if (scene::kind_of(c) == kind) {
            ++n;
        }
    }
    return n;
}

// 只数分组内部（即"数据图元"）的命令：轴/网格/标题/图例都在分组之外，
// 混在一起数会把轴线的 Stroke 算进来（轴线同样是真矢量描边）。
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

TEST_CASE("chart/scene: 折线图能产出真实 Scene") {
    LineChart chart = make_chart();
    scene::Scene s;
    CHECK(chart.to_scene(s, 50, 14));
    CHECK_FALSE(s.commands().empty());

    // 两条系列 → 两组，begin/end 各一条。
    CHECK(count_kind(s, scene::CommandKind::Group) == 4);
    // 折线是矢量描边，不是逐格 TextOp：每条系列一组 Stroke+MoveTo，段间用 LineTo。
    CHECK(count_grouped(s, scene::CommandKind::Stroke) == 2);
    CHECK(count_grouped(s, scene::CommandKind::MoveTo) == 2);
    CHECK(count_grouped(s, scene::CommandKind::LineTo) == 4);
    // 分组内部只允许"符号"（● 标记点，一格一个）退化成字符；数量必须与数据点
    // 同阶，绝不能随画布面积膨胀——那才说明它是逐格转录的假 Scene。
    const std::size_t grouped_total =
        count_grouped(s, scene::CommandKind::Stroke) + count_grouped(s, scene::CommandKind::MoveTo) +
        count_grouped(s, scene::CommandKind::LineTo) + count_grouped(s, scene::CommandKind::Text);
    CHECK(grouped_total == 2 + 2 + 4 + 6);  // 6 = 两条系列各 3 个标记点
    CHECK(count_grouped(s, scene::CommandKind::Text) <= 6);
    // 轴/刻度/标题/图例仍然是文本（字符艺术必须保留字符）：轴线若走矢量会被
    // 光栅成 ⣿/⠛ 的粗带，所以"整张图只有系列那 2 条 Stroke"是硬约束。
    CHECK(count_kind(s, scene::CommandKind::Text) > 0);
    CHECK(count_kind(s, scene::CommandKind::Stroke) == 2);
    CHECK(count_kind(s, scene::CommandKind::MoveTo) == 2);
}

TEST_CASE("chart/scene: 分组 begin/end 成对且 id 递增") {
    LineChart chart = make_chart();
    scene::Scene s;
    CHECK(chart.to_scene(s, 50, 14));

    std::vector<std::uint32_t> open;
    for (const auto& c : s.commands()) {
        const auto* g = std::get_if<scene::GroupOp>(&c);
        if (g == nullptr) {
            continue;
        }
        if (g->begin) {
            open.push_back(g->id);
        } else {
            REQUIRE_FALSE(open.empty());
            CHECK(open.back() == g->id);
            open.pop_back();
        }
    }
    CHECK(open.empty());  // 全部闭合，没有悬空的 begin
    CHECK(count_kind(s, scene::CommandKind::Group) == 4);
}

TEST_CASE("chart/scene: 未登记动画时两条系列都完整输出") {
    LineChart chart = make_chart();
    scene::Scene s;
    CHECK(chart.to_scene(s, 50, 14));

    // 每条系列 1 条 stroke + 1 个 move_to + (N-1) 个 line_to。
    CHECK(count_grouped(s, scene::CommandKind::LineTo) == 4);  // 两条各 2 段
}

TEST_CASE("chart/scene: 同一份 Scene 能被 render_scene 落到字符格") {
    LineChart chart = make_chart();
    scene::Scene s;
    CHECK(chart.to_scene(s, 50, 14));

    canvas::Canvas canvas(50, 14);
    canvas::render_scene(s, canvas);

    // 轴角是字符艺术，经 TextOp 落地后必须还在原位。
    bool corner = false;
    for (std::uint32_t y = 0; y < canvas.height() && !corner; ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            if (canvas.at(x, y).cp == U'\u2514') {  // └
                corner = true;
                break;
            }
        }
    }
    CHECK(corner);

    // 轴线必须是细的 ─，不能被盲文光栅成 ⠛/⣿ 的实心带。
    std::size_t dashes = 0;
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            if (canvas.at(x, y).cp == U'\u2500') {  // ─
                ++dashes;
            }
        }
    }
    CHECK(dashes >= 10);
}

TEST_CASE("chart/scene: 柱状图的柱是实心矩形，不是 █ 逐格转录") {
    chart::BarChart bar;
    bar.set_values({1.0, 2.0, 0.5}, {"x", "y", "z"}, Color::ansi16(2), "b");
    scene::Scene s;
    CHECK(bar.to_scene(s, 40, 12));

    // 一个 FillOp 状态 + 每类一根 RectOp；分组把它们收成"这条系列"。
    CHECK(count_kind(s, scene::CommandKind::Fill) == 1);
    CHECK(count_grouped(s, scene::CommandKind::Rect) == 3);
    CHECK(count_grouped(s, scene::CommandKind::Text) == 0);  // 柱里没有 █ 字符
    CHECK(count_kind(s, scene::CommandKind::Group) == 2);

    // 落到字符格后柱必须真的实心（半块光栅），否则"填充"只是空指令。
    canvas::Canvas canvas(40, 12);
    canvas::render_scene(s, canvas);
    std::size_t filled = 0;
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            const char32_t cp = canvas.at(x, y).cp;
            if (cp == U'\u2588' || cp == U'\u2580' || cp == U'\u2584') {  // █ ▀ ▄
                ++filled;
            }
        }
    }
    CHECK(filled > 0);
}

TEST_CASE("chart/scene: api 层把真实 Scene 一路导到 SVG") {
    chart::BarChart bar;
    bar.set_values({1.0, 2.0}, {"x", "y"}, Color::ansi16(2), "b");

    const auto sc = api::chart_to_scene(bar, 40, 12);
    REQUIRE(sc.has_value());
    CHECK_FALSE(sc->commands().empty());
    // 显式 0 尺寸是参数错误，不猜也不钳位。
    CHECK(api::chart_to_scene(bar, 0, 12).error() == ErrorCode::InvalidArgument);

    const auto svg = api::chart_to_svg(bar, 40, 12);
    REQUIRE(svg.has_value());
    const std::string& text = *svg;
    CHECK(text.find("<svg") != std::string::npos);
    CHECK(text.find("<rect") != std::string::npos);  // 柱是真正的 <rect fill>
    CHECK(text.find("<text") != std::string::npos);  // 轴标签仍是文本
}

TEST_CASE("chart/scene: 未迁移的图种在 api 层显式报 Unsupported") {
    struct Unmigrated final : chart::Chart {
        void draw(chart::DrawContext) override {}
    };
    Unmigrated chart;
    CHECK(api::chart_to_scene(chart, 40, 12).error() == ErrorCode::Unsupported);
    CHECK(api::chart_to_svg(chart, 40, 12).error() == ErrorCode::Unsupported);
}

TEST_CASE("chart/scene: 未迁移的图种默认返回 false，调用方可回退") {
    // 直接用 Chart 基类：默认 to_scene 返回 false，且不往 out 里写任何东西。
    struct Unmigrated final : chart::Chart {
        void draw(chart::DrawContext) override {}
    };
    Unmigrated chart;
    scene::Scene s;
    CHECK_FALSE(chart.to_scene(s, 40, 12));
    CHECK(s.commands().empty());
}
