// test_interaction_brush.cpp —— 刷选（吸附、去重、顺序、手势防呆）
#include <doctest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "termchart/interaction/brush.hpp"

namespace {

using termchart::Point;
using termchart::interaction::Brush;
using termchart::interaction::BrushOptions;
using Payloads = std::vector<std::uint32_t>;

std::vector<std::pair<Point, std::uint32_t>> sample_items() {
    return {
        {Point{0.0, 0.0}, 1},
        {Point{10.0, 0.0}, 2},
        {Point{10.0, 10.0}, 3},
        {Point{20.0, 0.0}, 2},  // payload 2 的第二个点
    };
}

}  // namespace

TEST_CASE("interaction/brush: 半径内吸附、半径外不吸附") {
    Brush b;
    b.set_items(sample_items());

    // 缺省半径 1.5：距点 (0,0) 0.5，吸附。
    b.begin(Point{0.5, 0.0});
    CHECK(b.active());
    CHECK(b.selected() == Payloads{1});

    // 距最近点 (10,0) 有 3，超出 1.5 —— 不吸附。
    Brush b2;
    b2.set_items(sample_items());
    b2.begin(Point{7.0, 0.0});
    CHECK(b2.selected().empty());

    // 自定义半径收紧到 0.1：0.5 的偏移就够不着了。
    Brush b3{BrushOptions{0.1}};
    b3.set_items(sample_items());
    b3.begin(Point{0.5, 0.0});
    CHECK(b3.selected().empty());
    CHECK(b3.options().radius == 0.1);
}

TEST_CASE("interaction/brush: 重复途经去重且保持首次吸附顺序") {
    Brush b;
    b.set_items(sample_items());

    b.begin(Point{0.0, 0.0});    // 吸附 payload 1
    b.stroke(Point{9.5, 0.0});   // 吸附 payload 2（距 (10,0) 0.5）
    b.stroke(Point{10.5, 0.0});  // 再途经 payload 2 —— 去重
    b.stroke(Point{10.0, 9.5});  // 吸附 payload 3
    b.stroke(Point{0.2, 0.0});   // 回头再刷 payload 1 —— 仍去重
    CHECK(b.selected() == Payloads{1, 2, 3});
    CHECK(b.selected(2));

    // payload 2 有两个点，途经另一个点也不重复记。
    b.stroke(Point{20.0, 0.0});
    CHECK(b.selected() == Payloads{1, 2, 3});

    b.end();
    CHECK_FALSE(b.active());
    CHECK(b.selected() == Payloads{1, 2, 3});  // 提笔后选中集保留
}

TEST_CASE("interaction/brush: clear 清空并退出手势") {
    Brush b;
    b.set_items(sample_items());
    b.begin(Point{0.0, 0.0});
    b.stroke(Point{10.0, 0.0});
    CHECK_FALSE(b.selected().empty());

    b.clear();
    CHECK(b.selected().empty());
    CHECK_FALSE(b.active());
    CHECK_FALSE(b.selected(1));

    // clear 之后未重新 begin：stroke 是 no-op。
    b.stroke(Point{10.0, 10.0});
    CHECK(b.selected().empty());
}

TEST_CASE("interaction/brush: set_items 后 begin 即吸附") {
    Brush b;
    b.begin(Point{10.0, 0.0});  // 先落笔、后给点集：落笔时刻点集为空，不吸附
    CHECK(b.selected().empty());

    b.set_items(sample_items());
    b.stroke(Point{0.2, 0.0});  // 途中给点集后继续刷，照常吸附
    CHECK(b.selected() == Payloads{1});
}

TEST_CASE("interaction/brush: 未 begin 时 stroke 是 no-op") {
    Brush b;
    b.set_items(sample_items());
    b.stroke(Point{0.0, 0.0});   // 直接途经数据点
    b.stroke(Point{10.0, 0.0});
    CHECK_FALSE(b.active());
    CHECK(b.selected().empty());
}

TEST_CASE("interaction/brush: 退化输入不吸附、不崩溃") {
    Brush b;
    b.set_items(sample_items());

    // 半径 NaN：永不吸附。
    Brush nan_radius{BrushOptions{std::numeric_limits<double>::quiet_NaN()}};
    nan_radius.set_items(sample_items());
    nan_radius.begin(Point{0.0, 0.0});
    CHECK(nan_radius.selected().empty());

    // 指针坐标 NaN：比较为 false，不吸附。
    b.begin(Point{std::numeric_limits<double>::quiet_NaN(), 0.0});
    CHECK(b.selected().empty());

    // 点坐标 Inf：同样够不着。
    Brush inf_items;
    inf_items.set_items({{Point{std::numeric_limits<double>::infinity(), 0.0}, 7}});
    inf_items.begin(Point{0.0, 0.0});
    CHECK(inf_items.selected().empty());
}
