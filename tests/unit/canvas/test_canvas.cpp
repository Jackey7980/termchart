// test_canvas.cpp —— Canvas（格粒度画布）与 Scene→Canvas 桥接
//
// 关键契约：
//   * 浮点坐标向下取整落格，越界静默丢弃（绘图路径不报错）；
//   * 宽字符写主码点 + 续接格，宽 0 的组合记号进侧面表（与 core/cell 一致）；
//   * render_scene 是终端路径上唯一解释 Scene 指令的地方。
#include <doctest.h>

#include <cmath>
#include <cstdint>
#include <string>

#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/primitives.hpp"
#include "termchart/canvas/scene_renderer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/style.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart/scene/scene.hpp"

namespace {

using namespace termchart;
using namespace termchart::canvas;
using namespace termchart::scene;

bool any_point_is(const std::vector<Point>& points, double x, double y) {
    for (const Point& p : points) {
        if (p.x == x && p.y == y) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST_CASE("canvas: 离散化产出格坐标且覆盖两端") {
    const std::vector<Point> points = line_points(Point{0.0, 0.0}, Point{4.0, 2.0});
    CHECK_FALSE(points.empty());
    CHECK(any_point_is(points, 0.0, 0.0));
    CHECK(any_point_is(points, 4.0, 2.0));
    for (const Point& p : points) {
        CHECK(p.x == std::floor(p.x));  // 已落格
        CHECK(p.y == std::floor(p.y));
    }
}

TEST_CASE("canvas: 退化成一点的线仍产出一格") {
    const std::vector<Point> points = line_points(Point{3.0, 3.0}, Point{3.0, 3.0});
    REQUIRE(points.size() == 1);
    CHECK(points[0].x == 3.0);
}

TEST_CASE("canvas: 矩形轮廓闭合，空矩形无点") {
    const std::vector<Point> outline = rect_outline_points(Rect{0.0, 0.0, 4.0, 3.0});
    CHECK(any_point_is(outline, 0.0, 0.0));
    CHECK(any_point_is(outline, 3.0, 0.0));
    CHECK(any_point_is(outline, 0.0, 2.0));
    CHECK(rect_outline_points(Rect{}).empty());
}

TEST_CASE("canvas: 圆的点都落在半径范围内") {
    const double radius = 3.0;
    const std::vector<Point> circle = circle_points(Point{5.0, 5.0}, radius);
    CHECK_FALSE(circle.empty());
    // 向下取整最多把一个点挪 √2 格，故容差取 (r + 2)²。
    const double limit = (radius + 2.0) * (radius + 2.0);
    for (const Point& p : circle) {
        const double dx = p.x - 5.0;
        const double dy = p.y - 5.0;
        CHECK(dx * dx + dy * dy <= limit);
    }
}

TEST_CASE("canvas: put 越界静默丢弃，不报错也不越界写") {
    Canvas canvas(4, 2);
    canvas.put(Point{100.0, 100.0}, U'x');
    canvas.put(Point{1.4, 1.9}, U'a');  // 向下取整 → (1,1)
    CHECK(canvas.at(1, 1).cp == U'a');
    CHECK(canvas.at(0, 0).cp == U' ');
}

TEST_CASE("canvas: fill_rect 只覆盖半开区间内的格") {
    Canvas canvas(6, 3);
    canvas.fill_rect(Rect{1.0, 1.0, 2.0, 1.0}, U'#');
    CHECK(canvas.at(1, 1).cp == U'#');
    CHECK(canvas.at(2, 1).cp == U'#');
    CHECK(canvas.at(3, 1).cp == U' ');  // right() 不含
    CHECK(canvas.at(1, 2).cp == U' ');  // bottom() 不含
}

TEST_CASE("canvas: draw_line 在格上连续无断口") {
    Canvas canvas(10, 3);
    canvas.draw_line(Point{0.0, 0.0}, Point{9.0, 2.0}, U'*');
    std::size_t drawn = 0;
    for (std::uint32_t x = 0; x < canvas.width(); ++x) {
        for (std::uint32_t y = 0; y < canvas.height(); ++y) {
            if (canvas.at(x, y).cp == U'*') {
                ++drawn;
            }
        }
    }
    CHECK(drawn >= 10);
}

TEST_CASE("canvas: draw_text 宽字符写主码点 + 续接格") {
    Canvas canvas(12, 1);
    canvas.draw_text(Point{0.0, 0.0}, "\u4e2d\u6587");
    CHECK(canvas.at(0, 0).cp == U'\u4e2d');
    CHECK(is_continuation(canvas.at(1, 0)));
    CHECK(canvas.at(2, 0).cp == U'\u6587');
    CHECK(is_continuation(canvas.at(3, 0)));
}

TEST_CASE("canvas: draw_text 组合记号进侧面表而不占格") {
    Canvas canvas(6, 1);
    canvas.draw_text(Point{0.0, 0.0}, "e\u0301x");
    CHECK(canvas.at(0, 0).cp == U'e');
    CHECK(canvas.at(1, 0).cp == U'x');  // 组合记号没占格
    CHECK(canvas.to_buffer().has_sides(0));
}

TEST_CASE("canvas: to_buffer 是快照，改画布不影响已取出的 buffer") {
    Canvas canvas(2, 1);
    canvas.put(0, 0, U'a');
    CellBuffer snapshot = canvas.to_buffer();
    canvas.put(0, 0, U'b');
    CHECK(snapshot.at(0, 0).cp == U'a');
    CHECK(canvas.at(0, 0).cp == U'b');
}

TEST_CASE("canvas: render_scene 把折线画进画布") {
    Scene scene;
    scene::SceneBuilder(scene)
        .move_to(0.0, 0.0)
        .line_to(5.0, 2.0)
        .stroke(Color::rgb(1, 2, 3));

    Canvas canvas(8, 4);
    render_scene(scene, canvas);

    std::size_t drawn = 0;
    for (std::uint32_t x = 0; x < canvas.width(); ++x) {
        for (std::uint32_t y = 0; y < canvas.height(); ++y) {
            if (canvas.at(x, y).cp == U'*') {
                ++drawn;
            }
        }
    }
    CHECK(drawn >= 6);
}

TEST_CASE("canvas: Fill 是状态指令 —— 作用于其后的矩形") {
    Scene scene;
    scene::SceneBuilder(scene)
        .fill(Color::ansi16(4))
        .rect(Rect{0.0, 0.0, 3.0, 2.0});

    Canvas canvas(5, 4);
    render_scene(scene, canvas);
    CHECK(canvas.at(0, 0).cp == U'\u2588');  // 填充用整块
    CHECK(canvas.at(2, 1).cp == U'\u2588');
    CHECK(canvas.at(3, 0).cp == U' ');
}

TEST_CASE("canvas: render_scene 的文本带着自己的样式") {
    Scene scene;
    scene::SceneBuilder(scene).text(Point{1.0, 1.0}, "hi",
                                    Style{}.set_fg(Color::rgb(9, 9, 9)).set_attr(Attr::Bold));

    Canvas canvas(6, 3);
    render_scene(scene, canvas);
    CHECK(canvas.at(1, 1).cp == U'h');
    CHECK(canvas.at(2, 1).cp == U'i');
    CHECK(canvas.at(1, 1).style.has_attr(Attr::Bold));
}
