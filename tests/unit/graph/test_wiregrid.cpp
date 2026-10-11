// test_wiregrid.cpp —— 连线占位栅格、箭头、标签避让、交叉计数
#include <doctest.h>

#include <cstddef>
#include <utility>
#include <vector>

#include "termchart/graph/arrow.hpp"
#include "termchart/graph/crossing.hpp"
#include "termchart/graph/label_place.hpp"
#include "termchart/graph/wiregrid.hpp"

namespace {

using namespace termchart;
using namespace termchart::graph;

}  // namespace

TEST_CASE("graph/wiregrid: 单格占用、释放与计数") {
    WireGrid grid(4, 3);
    CHECK(grid.empty());
    CHECK_FALSE(grid.occupied(0, 0));

    CHECK(grid.occupy(1, 1));
    CHECK(grid.occupied(1, 1));
    CHECK_FALSE(grid.occupy(1, 1));  // 重复占用失败
    CHECK(grid.occupied_count() == 1);
    CHECK_FALSE(grid.occupy(9, 9));  // 越界失败

    grid.release(1, 1);
    CHECK(grid.occupied_count() == 0);
    grid.release(1, 1);  // 重复释放安全
}

TEST_CASE("graph/wiregrid: 线段占用是原子的") {
    WireGrid grid(10, 5);
    CHECK(grid.occupy_h(2, 6, 1) == 5);
    CHECK(grid.occupied(2, 1));
    CHECK(grid.occupied(6, 1));

    // 与已有线段相交：整段失败，且不留下半截占用
    CHECK(grid.occupy_v(0, 4, 4) == 0);
    CHECK_FALSE(grid.occupied(4, 0));
    CHECK(grid.occupied_count() == 5);

    CHECK(grid.can_occupy_h(7, 9, 1));
    CHECK_FALSE(grid.can_occupy_h(5, 7, 1));

    CHECK(grid.occupy_v(0, 4, 8) == 5);
    grid.clear();
    CHECK(grid.empty());
}

TEST_CASE("graph/arrow: 头部字符与占格") {
    CHECK(arrow_head_char(PortSide::East) == U'\u25B6');
    CHECK(arrow_head_char(PortSide::West) == U'\u25C0');
    CHECK(arrow_head_char(PortSide::North) == U'\u25B2');
    CHECK(arrow_head_char(PortSide::South) == U'\u25BC');

    const std::vector<Point> east = arrow_head_cells(Point{5, 2}, PortSide::East);
    REQUIRE(east.size() == 2);
    CHECK(east[0].x == doctest::Approx(4.0));  // 靠后一格
    CHECK(east[1].x == doctest::Approx(5.0));  // tip
    CHECK(east[1].y == doctest::Approx(2.0));
}

TEST_CASE("graph/label_place: 依次尝试右、左、上、下") {
    LabelPlacement p = place_label(Point{5, 5}, 3, 1, {});
    CHECK(p.placed);
    CHECK(p.rect.x == doctest::Approx(6.0));
    CHECK(p.rect.y == doctest::Approx(5.0));

    // 右侧被占 → 左移
    p = place_label(Point{5, 5}, 3, 1, {Rect{6, 5, 3, 1}});
    CHECK(p.placed);
    CHECK(p.rect.x == doctest::Approx(1.0));  // 5 - 1 - 3
    CHECK(p.rect.y == doctest::Approx(5.0));

    // 右被占且左越界 → 上方
    p = place_label(Point{1, 5}, 4, 1, {Rect{2, 5, 4, 1}});
    CHECK(p.placed);
    CHECK(p.rect.x == doctest::Approx(1.0));
    CHECK(p.rect.y == doctest::Approx(3.0));  // 5 - 1 - 1
}

TEST_CASE("graph/crossing: 真交叉计数，共端点与共线不算") {
    CHECK(segments_cross(Point{0, 0}, Point{10, 0}, Point{5, -5}, Point{5, 5}));
    CHECK_FALSE(segments_cross(Point{0, 0}, Point{5, 5}, Point{0, 0}, Point{5, -5}));
    CHECK_FALSE(segments_cross(Point{0, 0}, Point{1, 0}, Point{0, 5}, Point{1, 5}));
    CHECK_FALSE(segments_cross(Point{0, 0}, Point{10, 0}, Point{2, 0}, Point{8, 0}));

    const std::vector<std::pair<Point, Point>> segments{
        {Point{0, 0}, Point{10, 10}},
        {Point{0, 10}, Point{10, 0}},
        {Point{0, 0}, Point{0, 10}},
    };
    // 前两条交叉；第三条与前两条共端点，只算 1
    CHECK(count_crossings(segments) == 1);
}
