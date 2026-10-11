// test_canvas_geometry.cpp —— 仿射变换、裁剪区域、扫描线填充
#include <doctest.h>

#include <cmath>
#include <cstddef>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/clip.hpp"
#include "termchart/canvas/fill.hpp"
#include "termchart/canvas/transform.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/util/geom.hpp"

namespace {

using namespace termchart;
using namespace termchart::canvas;

constexpr double kPi = 3.14159265358979323846;

std::vector<Point> square(double x, double y, double side) {
    return {Point{x, y}, Point{x + side, y}, Point{x + side, y + side},
            Point{x, y + side}};
}

}  // namespace

TEST_CASE("canvas/transform: 基本变换与复合顺序") {
    CHECK(Transform::translation(2.0, 3.0).apply(Point{1.0, 2.0}).x ==
          doctest::Approx(3.0));
    CHECK(Transform::translation(2.0, 3.0).apply(Point{1.0, 2.0}).y ==
          doctest::Approx(5.0));

    const Point rotated = Transform::rotation(kPi / 2.0).apply(Point{1.0, 0.0});
    CHECK(rotated.x == doctest::Approx(0.0));
    CHECK(rotated.y == doctest::Approx(1.0));

    // then 的语义是"先 this 后 next"：先平移再放大两倍
    const Transform chained = Transform::translation(1.0, 0.0).then(Transform::scaling(2.0, 2.0));
    const Point p = chained.apply(Point{1.0, 0.0});
    CHECK(p.x == doctest::Approx(4.0));
    CHECK(p.y == doctest::Approx(0.0));
}

TEST_CASE("canvas/transform: 求逆与不可逆情形") {
    const Transform t = Transform::translation(3.0, -4.0)
                            .then(Transform::rotation(0.7))
                            .then(Transform::scaling(2.0, 3.0));
    CHECK(t.invertible());
    const Point original{1.5, -2.5};
    const Point round_trip = t.inverse().apply(t.apply(original));
    CHECK(round_trip.x == doctest::Approx(original.x));
    CHECK(round_trip.y == doctest::Approx(original.y));

    const Transform degenerate = Transform::scaling(0.0, 1.0);
    CHECK_FALSE(degenerate.invertible());
    CHECK(degenerate.inverse().is_identity());  // 不可逆时退化为单位变换
}

TEST_CASE("canvas/transform: 包围盒含负缩放，宽高比补偿压 y") {
    // 负缩放会把矩形翻到另一侧，包围盒必须跟着走
    const Rect flipped =
        Transform::scaling(-1.0, 1.0).apply_bounds(Rect{0.0, 0.0, 10.0, 4.0});
    CHECK(flipped.x == doctest::Approx(-10.0));
    CHECK(flipped.w == doctest::Approx(10.0));
    CHECK(flipped.h == doctest::Approx(4.0));

    // 旋转 45° 后包围盒变大（对角撑开）
    const Rect rotated =
        Transform::rotation(kPi / 4.0).apply_bounds(Rect{0.0, 0.0, 1.0, 1.0});
    CHECK(rotated.w == doctest::Approx(std::sqrt(2.0)));

    // 宽高比 2 的字符格：y 压一半，几何圆才显圆
    const Point squashed = Transform::aspect_correction(2.0).apply(Point{0.0, 10.0});
    CHECK(squashed.y == doctest::Approx(5.0));
    CHECK(squashed.x == doctest::Approx(0.0));

    const std::vector<Point> points{Point{1.0, 1.0}, Point{2.0, 2.0}};
    CHECK(Transform::translation(1.0, 1.0).apply_all(points).size() == 2);
}

TEST_CASE("canvas/clip: 矩形区域包含判定与线段裁剪") {
    const ClipRegion region = ClipRegion::rect(Rect{2.0, 1.0, 4.0, 3.0});  // x∈[2,6], y∈[1,4]
    CHECK(region.contains(Point{3.0, 2.0}));
    CHECK(region.contains(Point{2.0, 1.0}));   // 边界算在内
    CHECK(region.contains(Point{6.0, 4.0}));
    CHECK_FALSE(region.contains(Point{1.9, 2.0}));
    CHECK_FALSE(region.contains(Point{3.0, 4.1}));
    CHECK(region.is_rect());

    Point a{};
    Point b{};
    REQUIRE(region.clip_segment(Point{0.0, 2.5}, Point{10.0, 2.5}, a, b));
    CHECK(a.x == doctest::Approx(2.0));
    CHECK(b.x == doctest::Approx(6.0));

    CHECK_FALSE(region.clip_segment(Point{0.0, 0.0}, Point{1.0, 0.0}, a, b));  // 全在外
    CHECK(region.clip_segment(Point{3.0, 2.0}, Point{5.0, 3.0}, a, b));         // 全在内
    CHECK(a.x == doctest::Approx(3.0));
    CHECK(b.x == doctest::Approx(5.0));

    // 无约束区域：全部放行
    const ClipRegion none;
    CHECK(none.unconstrained());
    CHECK(none.contains(Point{-100.0, -100.0}));
    CHECK(none.clip_segment(Point{0.0, 0.0}, Point{1.0, 1.0}, a, b));
}

TEST_CASE("canvas/clip: 矩形裁剪多边形（面积应等于矩形自身）") {
    const ClipRegion region = ClipRegion::rect(Rect{0.0, 0.0, 2.0, 2.0});
    const std::vector<Point> big = square(-5.0, -5.0, 20.0);
    const std::vector<Point> clipped = region.clip_polygon(big);
    REQUIRE(clipped.size() >= 3);
    CHECK(util::polygon_area(clipped.data(), clipped.size()) == doctest::Approx(4.0));

    // 完全在外：裁完为空
    CHECK(region.clip_polygon(square(10.0, 10.0, 2.0)).empty());
    // 完全在内：原样保留
    const std::vector<Point> inner = square(0.5, 0.5, 1.0);
    CHECK(region.clip_polygon(inner).size() == inner.size());
}

TEST_CASE("canvas/clip: 凸多边形区域；非凸只支持逐点判定") {
    const std::vector<Point> triangle{Point{0.0, 0.0}, Point{4.0, 0.0}, Point{0.0, 4.0}};
    const ClipRegion convex = ClipRegion::polygon(triangle);
    CHECK(convex.supports_polygon_clip());
    CHECK(convex.contains(Point{1.0, 1.0}));
    CHECK_FALSE(convex.contains(Point{3.0, 3.0}));  // 斜边之外

    Point a{};
    Point b{};
    REQUIRE(convex.clip_segment(Point{-1.0, 1.0}, Point{5.0, 1.0}, a, b));
    CHECK(a.x == doctest::Approx(0.0));
    CHECK(b.x == doctest::Approx(3.0));  // 斜边在 y=1 处交于 x=3

    CHECK_FALSE(convex.clip_segment(Point{3.0, 3.0}, Point{5.0, 5.0}, a, b));

    // 非凸（凹）多边形：不承诺多边形裁剪，明确报 false 并原样返回
    const std::vector<Point> concave{Point{0.0, 0.0}, Point{4.0, 0.0}, Point{4.0, 4.0},
                                     Point{2.0, 1.0}, Point{0.0, 4.0}};
    const ClipRegion region = ClipRegion::polygon(concave);
    CHECK_FALSE(region.supports_polygon_clip());
    const std::vector<Point> input = square(-1.0, -1.0, 10.0);
    CHECK(region.clip_polygon(input).size() == input.size());
    // 逐点判定仍然可用，且要认得那个凹口：顶点 (2,1) 把上边咬掉一块
    CHECK(region.contains(Point{2.0, 0.5}));        // 缺口下方：在内
    CHECK_FALSE(region.contains(Point{2.0, 3.5}));  // 缺口里：在外
    CHECK(region.contains(Point{0.5, 0.5}));        // 左下：在内
}

TEST_CASE("canvas/fill: 扫描线区间与实心填充") {
    const std::vector<Point> box = square(0.0, 0.0, 4.0);
    const std::vector<Span> spans = scanline_spans(box);
    REQUIRE(spans.size() == 4);  // 4 行，每行一段
    for (std::size_t i = 0; i < spans.size(); ++i) {
        CHECK(spans[i].row == i);
        CHECK(spans[i].x0 == doctest::Approx(0.0));
        CHECK(spans[i].x1 == doctest::Approx(4.0));
    }

    Canvas canvas(6, 6);
    CHECK(fill_polygon(canvas, box) == 16);  // 4×4
    CHECK(canvas.at(0, 0).cp == U'\u2588');
    CHECK(canvas.at(3, 3).cp == U'\u2588');
    CHECK(canvas.at(4, 4).cp == U' ');       // 边界外

    // 顶点数不足：什么都不画
    Canvas other(4, 4);
    CHECK(fill_polygon(other, {Point{0.0, 0.0}, Point{1.0, 1.0}}) == 0);
    CHECK(scanline_spans({Point{0.0, 0.0}, Point{1.0, 1.0}}).empty());
}

TEST_CASE("canvas/fill: 三角形逐行收窄，纵向超采样不改整格覆盖") {
    // 斜边从 (6,0) 走到 (0,6)：越往下越窄。
    const std::vector<Point> triangle{Point{0.0, 0.0}, Point{6.0, 0.0}, Point{0.0, 6.0}};
    Canvas canvas(8, 8);
    const std::size_t written = fill_polygon(canvas, triangle);
    CHECK(written > 0);
    CHECK(written < 36);  // 比同尺寸方形少（斜边吃掉了右上半区）

    std::size_t first_row_cells = 0;
    std::size_t last_row_cells = 0;
    for (std::uint32_t x = 0; x < canvas.width(); ++x) {
        if (canvas.at(x, 0).cp != U' ') {
            ++first_row_cells;
        }
        if (canvas.at(x, 5).cp != U' ') {
            ++last_row_cells;
        }
    }
    CHECK(first_row_cells >= 5);           // 顶行最宽
    CHECK(last_row_cells < first_row_cells);  // 底行最窄

    // 超采样：整格被覆盖时结果与单采样一致
    Canvas supersampled(8, 8);
    FillOptions opts;
    opts.supersample = 4;
    CHECK(fill_polygon(supersampled, square(0.0, 0.0, 4.0), opts) == 16);
}

TEST_CASE("canvas/fill: 负行被跳过而不是折回第 0 行") {
    const std::vector<Point> above{Point{0.0, -5.0}, Point{4.0, -5.0}, Point{4.0, -1.0},
                                   Point{0.0, -1.0}};
    const std::vector<Span> spans = scanline_spans(above);
    CHECK(spans.empty());  // 全在画布上方：一条区间都不产出

    Canvas canvas(6, 4);
    CHECK(fill_polygon(canvas, above) == 0);
    CHECK(canvas.at(0, 0).cp == U' ');
}
