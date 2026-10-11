// test_canvas_path_layer.cpp —— 路径对象（含曲线展平）与图层合成
#include <doctest.h>

#include <cmath>
#include <cstddef>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/layer.hpp"
#include "termchart/canvas/path.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"

namespace {

using namespace termchart;
using namespace termchart::canvas;

std::size_t count_nonblank(const Canvas& canvas) {
    std::size_t count = 0;
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            if (canvas.at(x, y).cp != U' ') {
                ++count;
            }
        }
    }
    return count;
}

}  // namespace

TEST_CASE("canvas/path: 矩形路径展平成闭合折线，包围盒覆盖四角") {
    Path path;
    path.add_rect(Rect{1.0, 2.0, 3.0, 4.0});
    CHECK_FALSE(path.empty());

    const std::vector<std::vector<Point>> polylines = path.flatten();
    REQUIRE(polylines.size() == 1);
    // 四个角 + 闭合回到起点
    CHECK(polylines[0].size() == 5);
    CHECK(polylines[0].front().x == doctest::Approx(polylines[0].back().x));
    CHECK(polylines[0].front().y == doctest::Approx(polylines[0].back().y));

    const Rect bounds = path.bounds();
    CHECK(bounds.x == doctest::Approx(1.0));
    CHECK(bounds.y == doctest::Approx(2.0));
    CHECK(bounds.w == doctest::Approx(3.0));
    CHECK(bounds.h == doctest::Approx(4.0));
}

TEST_CASE("canvas/path: 圆的展平点都落在半径附近，且点数随容差变化") {
    Path path;
    path.add_circle(Point{10.0, 10.0}, 4.0);

    const std::vector<std::vector<Point>> coarse = path.flatten(1.0);
    const std::vector<std::vector<Point>> fine = path.flatten(0.05);
    REQUIRE(coarse.size() == 1);
    REQUIRE(fine.size() == 1);
    CHECK(fine[0].size() > coarse[0].size());  // 容差越小点越多

    for (const Point& p : fine[0]) {
        const double distance = std::hypot(p.x - 10.0, p.y - 10.0);
        CHECK(distance == doctest::Approx(4.0).epsilon(0.05));
    }
}

TEST_CASE("canvas/path: 描边落在矩形轮廓上，内部保持空白") {
    Path path;
    path.add_rect(Rect{1.0, 2.0, 4.0, 3.0});

    Canvas canvas(8, 8);
    const std::size_t written = path.stroke(canvas, U'#');
    CHECK(written > 0);
    CHECK(canvas.at(1, 2).cp == U'#');   // 左上角
    CHECK(canvas.at(4, 2).cp == U'#');   // 右上角
    CHECK(canvas.at(1, 4).cp == U'#');   // 左下角
    CHECK(canvas.at(2, 3).cp == U' ');   // 内部不描边

    // 完全落在画布外的路径：一格都不写
    Path outside;
    outside.add_rect(Rect{100.0, 100.0, 4.0, 4.0});
    CHECK(outside.stroke(canvas, U'#') == 0);
}

TEST_CASE("canvas/path: 填充圆与矩形") {
    Path circle;
    circle.add_circle(Point{5.0, 5.0}, 3.0);
    Canvas canvas(11, 11);
    CHECK(circle.fill(canvas) > 0);
    CHECK(canvas.at(5, 5).cp == U'\u2588');   // 圆心在内部
    CHECK(canvas.at(0, 0).cp == U' ');        // 远角在外

    Path box;
    box.add_rect(Rect{0.0, 0.0, 3.0, 2.0});
    Canvas box_canvas(6, 4);
    CHECK(box.fill(box_canvas) == 6);  // 3×2

    const Path empty;
    Canvas empty_canvas(4, 4);
    CHECK(empty.flatten().empty());
    CHECK(empty.stroke(empty_canvas, U'#') == 0);
    CHECK(empty.fill(empty_canvas) == 0);
    CHECK(empty.bounds().w == doctest::Approx(0.0));
}

TEST_CASE("canvas/layer: Overlay 只写非空白，Replace 整层覆盖") {
    Layer layer(4, 3);
    layer.canvas().put(1, 1, U'x');

    Canvas target(6, 4);
    CHECK(layer.composite_to(target, Point{0.0, 0.0}) == 1);  // 只写了一格
    CHECK(target.at(1, 1).cp == U'x');
    CHECK(target.at(0, 0).cp == U' ');

    Canvas replaced(6, 4);
    CHECK(layer.composite_to(replaced, Point{0.0, 0.0}, Layer::Mode::Replace) == 12);
    CHECK(replaced.at(1, 1).cp == U'x');
    CHECK(replaced.at(0, 0).cp == U' ');
}

TEST_CASE("canvas/layer: 带样式的空格算内容（可铺底色）") {
    Layer layer(2, 2);
    const Style background = Style{}.set_bg(Color::ansi16(4));
    layer.canvas().put(0, 0, U' ', background);

    Canvas target(2, 2);
    CHECK(layer.composite_to(target, Point{0.0, 0.0}) == 1);
    CHECK(target.at(0, 0).style.bg() == Color::ansi16(4));
}

TEST_CASE("canvas/layer: Tint 换色保留字符；落点与负偏移按裁剪处理") {
    Layer layer(4, 2);
    layer.canvas().put(0, 0, U'a');
    layer.canvas().put(1, 0, U'b');

    Canvas tinted(4, 2);
    const Style tint = Style{}.set_fg(Color::ansi16(1));
    CHECK(layer.composite_to(tinted, Point{0.0, 0.0}, Layer::Mode::Tint, &tint) == 2);
    CHECK(tinted.at(0, 0).cp == U'a');
    CHECK(tinted.at(0, 0).style.fg() == Color::ansi16(1));

    Canvas shifted(6, 4);
    CHECK(layer.composite_to(shifted, Point{2.0, 1.0}) == 2);
    CHECK(shifted.at(2, 1).cp == U'a');
    CHECK(shifted.at(3, 1).cp == U'b');

    Canvas clipped(6, 4);
    CHECK(layer.composite_to(clipped, Point{-1.0, 0.0}) == 1);  // 左边一列被裁掉
    CHECK(clipped.at(0, 0).cp == U'b');
}

TEST_CASE("canvas/layer: dirty_bounds 给出最小重绘区域") {
    Layer layer(8, 4);
    CHECK(layer.dirty_bounds().w == doctest::Approx(0.0));  // 还没有基线
    layer.mark_clean();
    CHECK(layer.dirty_bounds().w == doctest::Approx(0.0));  // 无变化

    layer.canvas().put(2, 1, U'x');
    const Rect dirty = layer.dirty_bounds();
    CHECK(dirty.x == doctest::Approx(2.0));
    CHECK(dirty.y == doctest::Approx(1.0));
    CHECK(dirty.w == doctest::Approx(1.0));
    CHECK(dirty.h == doctest::Approx(1.0));

    layer.canvas().put(6, 3, U'y');
    const Rect wider = layer.dirty_bounds();
    CHECK(wider.x == doctest::Approx(2.0));
    CHECK(wider.y == doctest::Approx(1.0));
    CHECK(wider.w == doctest::Approx(5.0));  // x 从 2 到 6
    CHECK(wider.h == doctest::Approx(3.0));  // y 从 1 到 3

    layer.mark_clean();
    CHECK(layer.dirty_bounds().w == doctest::Approx(0.0));
    CHECK(count_nonblank(layer.canvas()) == 2);
}
