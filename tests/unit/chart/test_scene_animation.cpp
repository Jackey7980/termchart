// test_scene_animation.cpp —— Scene 层动画后处理拦截器
//
// 这一层要成立，必须同时满足两件事：
//   1. **组内数据**按进度改写（折线截断、柱长高）；
//   2. **组外一切**逐位不变（轴、刻度、网格、标题、图例不该跟着动）。
// 第 2 条比第 1 条更容易写错：一旦把轴也算进包围盒，截断线就会落在轴下方。
#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/chart/animation.hpp"
#include "termchart/chart/cartesian/bar_chart.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/chart/scene_animation.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"
#include "termchart/util/easing.hpp"

namespace {

using namespace termchart;
using namespace termchart::chart;

LineChart make_lines() {
    LineChart chart;
    chart.set_title("anim");
    Series a;
    a.name = "a";
    a.color = Color::ansi16(2);
    a.points = {{0.0, 0.0}, {1.0, 1.0}, {2.0, 4.0}, {3.0, 9.0}};
    Series b;
    b.name = "b";
    b.color = Color::ansi16(3);
    b.points = {{0.0, 9.0}, {1.0, 4.0}, {2.0, 1.0}, {3.0, 0.0}};
    chart.add_series(std::move(a));
    chart.add_series(std::move(b));
    return chart;
}

std::vector<double> group_xs(const scene::Scene& s, std::uint32_t group) {
    std::vector<double> xs;
    std::vector<std::uint32_t> stack;
    for (const auto& c : s.commands()) {
        if (const auto* g = std::get_if<scene::GroupOp>(&c); g != nullptr) {
            if (g->begin) {
                stack.push_back(g->id);
            } else if (!stack.empty()) {
                stack.pop_back();
            }
            continue;
        }
        if (stack.empty() || stack.back() != group) {
            continue;
        }
        if (const auto* m = std::get_if<scene::MoveTo>(&c); m != nullptr) {
            xs.push_back(m->to.x);
        } else if (const auto* l = std::get_if<scene::LineTo>(&c); l != nullptr) {
            xs.push_back(l->to.x);
        }
    }
    return xs;
}

std::vector<Rect> group_rects(const scene::Scene& s, std::uint32_t group) {
    std::vector<Rect> out;
    std::vector<std::uint32_t> stack;
    for (const auto& c : s.commands()) {
        if (const auto* g = std::get_if<scene::GroupOp>(&c); g != nullptr) {
            if (g->begin) {
                stack.push_back(g->id);
            } else if (!stack.empty()) {
                stack.pop_back();
            }
            continue;
        }
        if (stack.empty() || stack.back() != group) {
            continue;
        }
        if (const auto* r = std::get_if<scene::RectOp>(&c); r != nullptr) {
            out.push_back(r->rect);
        }
    }
    return out;
}

// 组外指令的完整快照：轴/网格/标题/图例必须逐位不变。
std::vector<scene::Command> outside_commands(const scene::Scene& s) {
    std::vector<scene::Command> out;
    int depth = 0;
    for (const auto& c : s.commands()) {
        if (const auto* g = std::get_if<scene::GroupOp>(&c); g != nullptr) {
            depth += g->begin ? 1 : -1;
            continue;
        }
        if (depth == 0) {
            out.push_back(c);
        }
    }
    return out;
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

std::size_t grouped_count(const scene::Scene& s, scene::CommandKind kind) {
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

TEST_CASE("chart/scene_animation: 进度 1 原样返回，进度 0 只清组内几何") {
    auto chart = make_lines();
    scene::Scene base;
    REQUIRE(chart.to_scene(base, 60, 18));

    scene::Scene copied = base;
    apply_animation(copied, 1.0);
    CHECK(copied == base);  // "动画结束" 必须逐位等于 "没有动画"

    scene::Scene out_of_range = base;
    apply_animation(out_of_range, 1.7);  // >1 同样视作完成
    CHECK(out_of_range == base);

    scene::Scene zeroed = base;
    apply_animation(zeroed, 0.0);
    // 组内几何全空（两条线 + 标记点），但组标记和组外的轴/标题仍在。
    CHECK(grouped_count(zeroed, scene::CommandKind::MoveTo) == 0);
    CHECK(grouped_count(zeroed, scene::CommandKind::LineTo) == 0);
    CHECK(grouped_count(zeroed, scene::CommandKind::Text) == 0);
    CHECK(count_kind(zeroed, scene::CommandKind::Group) == 4);  // 2 组 × begin/end
    CHECK(zeroed.size() > 0);
    CHECK(outside_commands(zeroed) == outside_commands(base));
}

TEST_CASE("chart/scene_animation: 折线按 X 截断，轴与网格不动") {
    auto chart = make_lines();
    scene::Scene s;
    REQUIRE(chart.to_scene(s, 60, 18));

    const std::vector<scene::Command> axis_before = outside_commands(s);
    const std::vector<double> xs = group_xs(s, 0);
    REQUIRE(xs.size() >= 4);
    double min_x = xs.front();
    double max_x = xs.front();
    for (double x : xs) {
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
    }

    apply_animation(s, 0.5);

    const double cut = min_x + (max_x - min_x) * 0.5;
    const std::vector<double> clipped = group_xs(s, 0);
    REQUIRE(clipped.size() >= 2);  // 不能把整条线删空
    for (double x : clipped) {
        CHECK(x <= doctest::Approx(cut).epsilon(1e-9));
    }
    // 末尾点必须落在截断线上（插值端点），否则视觉上会"提前断线"。
    CHECK(clipped.back() == doctest::Approx(cut).epsilon(1e-9));
    // 起点没被挪动。
    CHECK(clipped.front() == doctest::Approx(min_x).epsilon(1e-9));

    // 组外逐位不变：轴/网格/标题都在 GroupOp 之外，动画不许碰它们。
    CHECK(outside_commands(s) == axis_before);
}

TEST_CASE("chart/scene_animation: 柱固定基线长高（正值向上、负值向下）") {
    BarChart bar;
    bar.set_values({2.0, 5.0, 3.0}, {"a", "b", "c"}, Color::ansi16(2), "v");
    scene::Scene s;
    REQUIRE(bar.to_scene(s, 40, 12));
    const std::vector<Rect> before = group_rects(s, 0);
    REQUIRE(before.size() == 3);

    apply_animation(s, 0.5);
    const std::vector<Rect> after = group_rects(s, 0);
    REQUIRE(after.size() == before.size());
    for (std::size_t i = 0; i < before.size(); ++i) {
        // 高度减半、宽度不变、**底边不动**（正柱从 0 线往上长）。
        CHECK(after[i].h == doctest::Approx(before[i].h * 0.5));
        CHECK(after[i].w == doctest::Approx(before[i].w));
        CHECK(after[i].bottom() == doctest::Approx(before[i].bottom()));
    }

    // 负值：矩形的顶边固定在 0 线上，向下长——同一套公式，不需要分支。
    BarChart negative;
    negative.set_values({-2.0, -5.0}, {"a", "b"}, Color::ansi16(1), "neg");
    scene::Scene ns;
    REQUIRE(negative.to_scene(ns, 40, 12));
    const std::vector<Rect> nbefore = group_rects(ns, 0);
    REQUIRE(nbefore.size() == 2);
    apply_animation(ns, 0.25);
    const std::vector<Rect> nafter = group_rects(ns, 0);
    REQUIRE(nafter.size() == nbefore.size());
    for (std::size_t i = 0; i < nbefore.size(); ++i) {
        CHECK(nafter[i].y == doctest::Approx(nbefore[i].y));  // 顶边 = 0 线
        CHECK(nafter[i].h == doctest::Approx(nbefore[i].h * 0.25));
    }
}

TEST_CASE("chart/scene_animation: 按组进度互不影响，表里没有的组全显示") {
    auto chart = make_lines();
    scene::Scene s;
    REQUIRE(chart.to_scene(s, 60, 18));

    const std::vector<double> xs0 = group_xs(s, 0);
    const std::vector<double> xs1 = group_xs(s, 1);
    REQUIRE(!xs0.empty());
    REQUIRE(!xs1.empty());

    const std::vector<GroupProgress> groups{{0, 0.5}};  // 只给组 0
    apply_animation(s, groups);

    const std::vector<double> c0 = group_xs(s, 0);
    const std::vector<double> c1 = group_xs(s, 1);
    CHECK(c0.size() < xs0.size());  // 组 0 被截
    CHECK(c1 == xs1);               // 组 1 不在表里 → 原样
}

TEST_CASE("chart/animation_state: tick(dt) 增量步进") {
    AnimationState state;  // 默认 0.5s，EaseOutCubic
    CHECK(state.progress() == doctest::Approx(1.0));  // 未启动 = 全显示
    CHECK_FALSE(state.started());

    state.tick(0.0);  // 首帧：只启动，不推进
    CHECK(state.started());
    CHECK(state.progress() == doctest::Approx(0.0));

    state.tick(0.1);
    CHECK(state.progress() ==
          doctest::Approx(util::ease(util::Easing::EaseOutCubic, 0.2)));

    state.tick(0.4);
    CHECK(state.progress() == doctest::Approx(1.0));
    CHECK(state.finished());

    state.tick(-1.0);  // 时间倒流：不推进、不倒退
    CHECK(state.progress() == doctest::Approx(1.0));

    state.reset();
    CHECK(state.progress() == doctest::Approx(1.0));  // reset 回到"未启动"

    // 延迟 + 线性：延迟内 0，之后按比例。
    AnimationState delayed{AnimationTrack{0.2, 0.1, util::Easing::Linear}};
    delayed.tick(0.05);
    CHECK(delayed.progress() == doctest::Approx(0.0));
    delayed.tick(0.10);  // elapsed 0.15 → active 0.05 / 0.2 = 0.25
    CHECK(delayed.progress() == doctest::Approx(0.25));

    // 零时长：延迟一过立刻完成（首帧不能卡在 0）。
    AnimationState instant{AnimationTrack{0.0, 0.0, util::Easing::Linear}};
    instant.tick(0.0);
    CHECK(instant.progress() == doctest::Approx(1.0));
    CHECK(instant.finished());
}
