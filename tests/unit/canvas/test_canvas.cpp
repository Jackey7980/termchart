// test_canvas.cpp —— Canvas（格粒度画布）与 Scene→Canvas 桥接
//
// 关键契约：
//   * 浮点坐标向下取整落格，越界静默丢弃（绘图路径不报错）；
//   * 负坐标是**裁剪**而非折回第 0 列/行；极大/Inf/NaN 坐标饱和或丢弃，绝不越界写；
//   * 宽字符写主码点 + 续接格，宽 0 的组合记号进侧面表（与 core/cell 一致）；
//   * render_scene 是终端路径上唯一解释 Scene 指令的地方。
#include <doctest.h>

#include <cstddef>
#include <limits>

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

TEST_CASE("canvas: 负坐标被裁剪而非折回第 0 列") {
    Canvas canvas(6, 2);
    canvas.put(Point{-5.0, 0.0}, U'x');
    CHECK(canvas.at(0, 0).cp == U' ');  // 不折回边界

    canvas.draw_text(Point{-2.0, 0.0}, "abcdef", Style{});
    CHECK(canvas.at(0, 0).cp == U'c');  // 起点前的两字被裁掉
    CHECK(canvas.at(3, 0).cp == U'f');
}

TEST_CASE("canvas: 极大 / Inf / NaN 坐标不越界写") {
    Canvas canvas(4, 2);
    canvas.put(Point{1e18, 1e18}, U'x');
    canvas.put(Point{std::numeric_limits<double>::infinity(), 0.0}, U'y');
    canvas.put(Point{std::numeric_limits<double>::quiet_NaN(), 0.0}, U'z');
    canvas.fill_rect(Rect{1e18, 1e18, 1e18, 1e18}, U'#');
    canvas.draw_text(Point{std::numeric_limits<double>::quiet_NaN(), 0.0}, "q", Style{});

    std::size_t nonblank = 0;
    for (std::uint32_t y = 0; y < canvas.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            if (canvas.at(x, y).cp != U' ') {
                ++nonblank;
            }
        }
    }
    CHECK(nonblank == 0);
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

TEST_CASE("canvas: render_scene 把折线画成盲文子像素，占位符 '*' 彻底消失") {
    Scene scene;
    // 状态在前、几何在后：Fill/Stroke 只作用于其后出现的图元（见 scene_renderer.cpp 注释）。
    scene::SceneBuilder(scene)
        .stroke(Color::rgb(1, 2, 3))
        .move_to(0.0, 0.0)
        .line_to(5.0, 2.0);

    Canvas canvas(8, 4);
    render_scene(scene, canvas);

    std::size_t dots = 0;
    std::size_t placeholders = 0;
    bool stroke_colored = false;
    for (std::uint32_t x = 0; x < canvas.width(); ++x) {
        for (std::uint32_t y = 0; y < canvas.height(); ++y) {
            const Cell& cell = canvas.at(x, y);
            if (cell.cp >= U'\u2800' && cell.cp <= U'\u28FF') {  // 盲文点阵区
                ++dots;
                if (cell.style.fg() == Color::rgb(1, 2, 3)) {
                    stroke_colored = true;
                }
            }
            if (cell.cp == U'*') {
                ++placeholders;
            }
        }
    }
    CHECK(dots >= 6);         // 描边落到了子像素上
    CHECK(placeholders == 0); // 不再是 kStrokeChar 占位
    CHECK(stroke_colored);    // 颜色来自 StrokeOp
}

TEST_CASE("canvas: render_scene 的圆弧也是盲文子像素") {
    Scene scene;
    scene::SceneBuilder(scene)
        .stroke(Color::ansi16(2))
        .arc(Point{6.0, 4.0}, 4.0, 0.0, 6.283185307179586);

    Canvas canvas(16, 10);
    render_scene(scene, canvas);

    std::size_t dots = 0;
    for (std::uint32_t x = 0; x < canvas.width(); ++x) {
        for (std::uint32_t y = 0; y < canvas.height(); ++y) {
            const char32_t cp = canvas.at(x, y).cp;
            if (cp >= U'\u2800' && cp <= U'\u28FF') {
                ++dots;
            }
            CHECK_FALSE(cp == U'*');
        }
    }
    CHECK(dots >= 8);  // 一个圆至少要落到多个格上
}

TEST_CASE("canvas: 多次收口互不擦除（盲文空白不算内容）") {
    Scene scene;
    scene::SceneBuilder(scene)
        .stroke(Color::ansi16(2))
        .move_to(0.0, 0.0)
        .line_to(5.0, 0.0)
        .stroke(Color::ansi16(4))  // 换色触发第一次收口
        .move_to(0.0, 3.0)
        .line_to(5.0, 3.0);

    Canvas canvas(8, 5);
    render_scene(scene, canvas);

    // 只统计"真的有点"的盲文格；U+2800 是空白，不算内容。
    const auto dots_in_row = [&canvas](std::uint32_t y) {
        std::size_t count = 0;
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            const char32_t cp = canvas.at(x, y).cp;
            if (cp > U'\u2800' && cp <= U'\u28FF') {
                ++count;
            }
        }
        return count;
    };
    const auto blank_braille_in_row = [&canvas](std::uint32_t y) {
        std::size_t count = 0;
        for (std::uint32_t x = 0; x < canvas.width(); ++x) {
            if (canvas.at(x, y).cp == U'\u2800') {
                ++count;
            }
        }
        return count;
    };

    CHECK(dots_in_row(0) > 0);  // 第一条线还在
    CHECK(dots_in_row(3) > 0);  // 第二条线画上了
    CHECK(blank_braille_in_row(0) == 0);  // 第二次收口没有把空白铺回来
}

TEST_CASE("canvas: Stroke 是状态指令 —— 画在其前的几何用旧颜色") {
    Scene scene;
    scene::SceneBuilder(scene)
        .move_to(0.0, 0.0)  // 几何在前
        .line_to(5.0, 0.0)
        .stroke(Color::rgb(200, 0, 0));  // 之后才设色

    Canvas canvas(8, 3);
    render_scene(scene, canvas);

    std::size_t dots = 0;
    bool got_new_color = false;
    for (std::uint32_t x = 0; x < canvas.width(); ++x) {
        for (std::uint32_t y = 0; y < canvas.height(); ++y) {
            const Cell& cell = canvas.at(x, y);
            if (cell.cp >= U'\u2800' && cell.cp <= U'\u28FF') {
                ++dots;
                if (cell.style.fg() == Color::rgb(200, 0, 0)) {
                    got_new_color = true;
                }
            }
        }
    }
    CHECK(dots >= 4);
    CHECK_FALSE(got_new_color);  // 后设的颜色不该回头染上已画出的线
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
