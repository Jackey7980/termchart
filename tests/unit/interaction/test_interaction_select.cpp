// test_interaction_select.cpp —— 框选状态机
#include <doctest.h>

#include <cstdint>
#include <vector>

#include "termchart/interaction/select.hpp"

namespace {

using termchart::Point;
using termchart::Rect;
using termchart::interaction::BoxSelect;

}  // namespace

TEST_CASE("interaction/select: 正向拖拽 → end 后矩形保留供查询") {
    BoxSelect s;
    CHECK_FALSE(s.active());
    CHECK_FALSE(s.has_rect());
    CHECK(s.rect() == Rect{});  // 无矩形返回 {}

    s.begin(Point{10.0, 10.0});
    CHECK(s.active());
    // 只 begin 未拖动：还没有矩形（纯点击不算框选）。
    CHECK_FALSE(s.has_rect());

    s.update(Point{30.0, 25.0});
    CHECK(s.has_rect());
    CHECK(s.rect() == Rect{10.0, 10.0, 20.0, 15.0});

    s.end();
    CHECK_FALSE(s.active());
    CHECK(s.has_rect());  // 松开后矩形仍可查询。
    CHECK(s.rect() == Rect{10.0, 10.0, 20.0, 15.0});
}

TEST_CASE("interaction/select: 反向拖拽仍归一化成正矩形") {
    BoxSelect s;
    // 从右下往左上拖。
    s.begin(Point{30.0, 25.0});
    s.update(Point{10.0, 10.0});
    CHECK(s.rect() == Rect{10.0, 10.0, 20.0, 15.0});

    // 反向改拖一半、且两轴方向不同（左下 → 右上）。
    BoxSelect s2;
    s2.begin(Point{30.0, 10.0});
    s2.update(Point{10.0, 25.0});
    CHECK(s2.rect() == Rect{10.0, 10.0, 20.0, 15.0});
}

TEST_CASE("interaction/select: 未 begin 时 update/end 都是 no-op") {
    BoxSelect s;
    s.update(Point{50.0, 50.0});
    s.end();
    CHECK_FALSE(s.active());
    CHECK_FALSE(s.has_rect());
    CHECK(s.rect() == Rect{});

    // end 之后再来一条 update 也不得"复活"矩形。
    s.begin(Point{0.0, 0.0});
    s.update(Point{10.0, 10.0});
    s.end();
    s.update(Point{20.0, 20.0});
    CHECK_FALSE(s.active());
    CHECK(s.rect() == Rect{0.0, 0.0, 10.0, 10.0});
}

TEST_CASE("interaction/select: cancel 清空；重复 begin 重置起点") {
    // cancel：任何时刻（拖拽中/松开后）都清空矩形。
    BoxSelect s;
    s.begin(Point{0.0, 0.0});
    s.update(Point{10.0, 10.0});
    s.cancel();
    CHECK_FALSE(s.active());
    CHECK_FALSE(s.has_rect());
    CHECK(s.rect() == Rect{});

    // 松开后再 cancel 一样清空。
    s.begin(Point{0.0, 0.0});
    s.update(Point{10.0, 10.0});
    s.end();
    s.cancel();
    CHECK_FALSE(s.has_rect());
    CHECK(s.rect() == Rect{});

    // 拖拽中重复 begin = 重新起笔：旧矩形作废，直到下一次 update。
    BoxSelect s2;
    s2.begin(Point{0.0, 0.0});
    s2.update(Point{50.0, 50.0});
    CHECK(s2.has_rect());
    s2.begin(Point{10.0, 10.0});
    CHECK(s2.active());
    CHECK_FALSE(s2.has_rect());
    s2.update(Point{20.0, 20.0});
    CHECK(s2.rect() == Rect{10.0, 10.0, 10.0, 10.0});
}

TEST_CASE("interaction/select: payloads_inside 的闭-开边界与输入顺序") {
    BoxSelect s;
    s.begin(Point{10.0, 10.0});
    s.update(Point{30.0, 25.0});

    const std::vector<std::pair<Point, std::uint32_t>> items{
        {Point{10.0, 10.0}, 1},    // 恰在左上角：含（闭-开区间含左/上边界）
        {Point{29.9, 24.9}, 2},    // 框内
        {Point{30.0, 20.0}, 3},    // 恰在右边界：不含
        {Point{20.0, 25.0}, 4},    // 恰在下边界：不含
        {Point{30.0, 25.0}, 5},    // 右下角：两轴都不含
        {Point{9.9, 10.0}, 6},     // 左边界外
        {Point{20.0, 9.9}, 7},     // 上边界外
        {Point{100.0, 100.0}, 8},  // 框外
    };
    // 框内的按输入顺序返回（乱序给出以验证保序）。
    const std::vector<std::pair<Point, std::uint32_t>> shuffled{
        items[2], items[0], items[7], items[1], items[4], items[3], items[6], items[5],
    };
    const std::vector<std::uint32_t> got = s.payloads_inside(shuffled);
    REQUIRE(got.size() == 2);
    CHECK(got[0] == 1);
    CHECK(got[1] == 2);
}

TEST_CASE("interaction/select: 无矩形时 payloads_inside 返回空") {
    BoxSelect s;
    const std::vector<std::pair<Point, std::uint32_t>> items{{Point{0.0, 0.0}, 1}};

    // 从未 begin。
    CHECK(s.payloads_inside(items).empty());

    // 只有 begin（零面积矩形不算框选）。
    s.begin(Point{5.0, 5.0});
    CHECK(s.payloads_inside(items).empty());

    // cancel 之后也不残留结果。
    s.update(Point{10.0, 10.0});
    s.cancel();
    CHECK(s.payloads_inside(items).empty());
}
