// test_geom.cpp —— 平面几何
#include <doctest.h>

#include <cmath>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/util/geom.hpp"

namespace {

using namespace termchart;
using namespace termchart::util;

}  // namespace

TEST_CASE("util/geom: 叉积与带符号距离的左右侧约定一致") {
    const Point a{0.0, 0.0};
    const Point b{1.0, 0.0};
    const Point above{0.0, 1.0};
    const Point below{0.0, -1.0};

    CHECK(cross(a, b, above) > 0.0);  // a→b→above 左转
    CHECK(cross(a, b, below) < 0.0);
    CHECK(cross(a, b, Point{2.0, 0.0}) == doctest::Approx(0.0));

    CHECK(signed_distance_to_line(above, a, b) == doctest::Approx(1.0));
    CHECK(signed_distance_to_line(below, a, b) == doctest::Approx(-1.0));
    // 线段退化成点：返回点距，恒非负
    CHECK(signed_distance_to_line(Point{3.0, 4.0}, a, a) == doctest::Approx(5.0));
}

TEST_CASE("util/geom: 点到线段距离，投影落在端点之外取端点距") {
    const Point a{0.0, 0.0};
    const Point b{10.0, 0.0};

    CHECK(distance_to_segment(Point{5.0, 3.0}, a, b) == doctest::Approx(3.0));
    CHECK(distance_to_segment(Point{-3.0, 4.0}, a, b) == doctest::Approx(5.0));
    CHECK(distance_to_segment(Point{14.0, 3.0}, a, b) == doctest::Approx(5.0));
    CHECK(distance_to_segment(Point{1.0, 1.0}, a, a) == doctest::Approx(std::sqrt(2.0)));
}

TEST_CASE("util/geom: 严格相交 —— 共端点、贴线、共线、平行都不算") {
    const Point a{0.0, 0.0};
    const Point b{10.0, 10.0};
    CHECK(segments_cross(a, b, Point{0.0, 10.0}, Point{10.0, 0.0}));  // X 形

    CHECK_FALSE(segments_cross(a, b, b, Point{20.0, 0.0}));  // 共端点
    CHECK_FALSE(segments_cross(a, b, Point{5.0, 5.0}, Point{20.0, 5.0}));  // 端点落在中段
    CHECK_FALSE(segments_cross(a, b, a, Point{5.0, 5.0}));   // 共线重叠
    CHECK_FALSE(segments_cross(a, b, Point{0.0, 1.0}, Point{10.0, 11.0}));  // 平行
}

TEST_CASE("util/geom: 线段交点只在两段之内才返回 true") {
    Point out{};
    REQUIRE(segment_intersection(Point{0.0, 0.0}, Point{10.0, 10.0}, Point{0.0, 10.0},
                                 Point{10.0, 0.0}, out));
    CHECK(out.x == doctest::Approx(5.0));
    CHECK(out.y == doctest::Approx(5.0));

    CHECK_FALSE(segment_intersection(Point{0.0, 0.0}, Point{1.0, 0.0}, Point{0.0, 1.0},
                                     Point{1.0, 1.0}, out));  // 平行
    CHECK_FALSE(segment_intersection(Point{0.0, 0.0}, Point{1.0, 0.0}, Point{0.0, 0.0},
                                     Point{1.0, 0.0}, out));  // 共线
    CHECK_FALSE(segment_intersection(Point{0.0, 0.0}, Point{1.0, 1.0}, Point{5.0, 0.0},
                                     Point{6.0, 1.0}, out));  // 只有延长线相交
    CHECK_FALSE(segment_intersection(Point{0.0, 0.0}, Point{0.0, 0.0}, Point{1.0, 0.0},
                                     Point{1.0, 1.0}, out));  // 退化线段
}

TEST_CASE("util/geom: 多边形面积带符号、重心按面积加权") {
    const std::vector<Point> ccw{{0.0, 0.0}, {4.0, 0.0}, {4.0, 3.0}, {0.0, 3.0}};
    CHECK(polygon_area(ccw.data(), ccw.size()) == doctest::Approx(12.0));

    const std::vector<Point> cw{ccw.rbegin(), ccw.rend()};
    CHECK(polygon_area(cw.data(), cw.size()) == doctest::Approx(-12.0));

    const Point center = polygon_centroid(ccw.data(), ccw.size());
    CHECK(center.x == doctest::Approx(2.0));
    CHECK(center.y == doctest::Approx(1.5));

    CHECK(polygon_area(nullptr, 0) == doctest::Approx(0.0));
    // 面积为零（退化成线段）时退回顶点平均，结果仍稳定
    const std::vector<Point> line{{0.0, 0.0}, {10.0, 0.0}};
    CHECK(polygon_centroid(line.data(), line.size()).x == doctest::Approx(5.0));
}

TEST_CASE("util/geom: 点在多边形内，边界算内部") {
    const std::vector<Point> square{{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}};
    CHECK(point_in_polygon(Point{5.0, 5.0}, square.data(), square.size()));
    CHECK_FALSE(point_in_polygon(Point{15.0, 5.0}, square.data(), square.size()));
    CHECK_FALSE(point_in_polygon(Point{5.0, -1.0}, square.data(), square.size()));
    CHECK(point_in_polygon(Point{0.0, 5.0}, square.data(), square.size()));
    CHECK(point_in_polygon(Point{10.0, 10.0}, square.data(), square.size()));
    CHECK_FALSE(point_in_polygon(Point{5.0, 5.0}, square.data(), 2));  // 顶点不足
}

TEST_CASE("util/geom: 凸包逆时针，剔除内部点与共线中间点") {
    const std::vector<Point> points{{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {0.0, 4.0},
                                    {2.0, 2.0}, {2.0, 0.0}, {0.0, 2.0}};
    const std::vector<Point> hull = convex_hull(points);
    REQUIRE(hull.size() == 4);
    CHECK(polygon_area(hull.data(), hull.size()) > 0.0);  // 逆时针

    for (const Point& p : hull) {
        const bool is_interior_point = (p.x == 2.0 && p.y == 2.0);
        CHECK_FALSE(is_interior_point);
    }
    CHECK(convex_hull(std::vector<Point>{{1.0, 1.0}}).size() == 1);
    CHECK(convex_hull(std::vector<Point>{{1.0, 1.0}, {2.0, 2.0}}).size() == 2);
    CHECK(convex_hull(std::vector<Point>{}).empty());
}
