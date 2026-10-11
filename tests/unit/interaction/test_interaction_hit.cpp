// test_interaction_hit.cpp —— HitRegion 几何、命中测试、焦点链
#include <doctest.h>

#include <cstdint>
#include <vector>

#include "termchart/chart/hit_region.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/interaction/focus.hpp"
#include "termchart/interaction/hit_test.hpp"

namespace {

using namespace termchart;
using namespace termchart::interaction;

}  // namespace

TEST_CASE("interaction/hit: HitRegion 三种形状的几何判定") {
    const auto rect = chart::HitRegion::rect(Rect{0, 0, 10, 10}, 1);
    CHECK(rect.shape() == chart::HitShape::Rect);
    CHECK(rect.contains(5, 5));
    CHECK_FALSE(rect.contains(11, 5));

    const auto circle = chart::HitRegion::circle(Point{0, 0}, 5.0, 2);
    CHECK(circle.shape() == chart::HitShape::Circle);
    CHECK(circle.contains(3, 4));   // 3-4-5
    CHECK_FALSE(circle.contains(4, 4));

    const auto poly = chart::HitRegion::polygon({{0, 0}, {10, 0}, {0, 10}}, 3);
    CHECK(poly.shape() == chart::HitShape::Polygon);
    CHECK(poly.contains(1, 1));
    CHECK_FALSE(poly.contains(9, 9));
}

TEST_CASE("interaction/hit: topmost 取最后登记，payloads 汇总全部命中") {
    std::vector<chart::HitRegion> regions;
    regions.push_back(chart::HitRegion::rect(Rect{0, 0, 10, 10}, 10));
    regions.push_back(chart::HitRegion::rect(Rect{0, 0, 5, 5}, 20));

    const chart::HitRegion* top = topmost_hit(regions, 1, 1);
    REQUIRE(top != nullptr);
    CHECK(top->payload() == 20);  // 后登记在上层

    const chart::HitRegion* only_big = topmost_hit(regions, 7, 7);
    REQUIRE(only_big != nullptr);
    CHECK(only_big->payload() == 10);

    CHECK(topmost_hit(regions, 99, 99) == nullptr);

    const std::vector<std::uint32_t> ids = hit_payloads(regions, 1, 1);
    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == 10);  // 自下而上
    CHECK(ids[1] == 20);
}

TEST_CASE("interaction/focus: 前后移动、环绕与清空") {
    FocusChain focus;
    CHECK(focus.current() == 0);
    CHECK_FALSE(focus.is_focused(0));

    const std::vector<std::uint32_t> order{1, 2, 3};
    focus.next(order);
    CHECK(focus.current() == 1);  // 无焦点 → 落到首项
    focus.next(order);
    CHECK(focus.current() == 2);
    focus.next(order);
    CHECK(focus.current() == 3);
    focus.next(order);
    CHECK(focus.current() == 1);  // 环绕

    focus.prev(order);
    CHECK(focus.current() == 3);
    CHECK(focus.is_focused(3));
    CHECK_FALSE(focus.is_focused(1));

    focus.clear();
    CHECK(focus.current() == 0);
}
