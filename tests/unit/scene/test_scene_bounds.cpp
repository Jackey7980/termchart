// test_scene_bounds.cpp —— Scene 几何包围盒
#include <doctest.h>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/scene/bounds.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"

namespace {

using namespace termchart;
using namespace termchart::scene;

}  // namespace

TEST_CASE("scene/bounds: 空 Scene 是零矩形；状态指令不产生几何") {
    Scene empty;
    CHECK(scene_bounds(empty).w == doctest::Approx(0.0));
    CHECK(scene_bounds(empty).h == doctest::Approx(0.0));

    Scene states_only;
    states_only.push(FillOp{Color::ansi16(1)});
    states_only.push(StrokeOp{Color::ansi16(2), 2.0});
    const Rect bounds = scene_bounds(states_only);
    CHECK(bounds.w == doctest::Approx(0.0));
    CHECK(bounds.h == doctest::Approx(0.0));
}

TEST_CASE("scene/bounds: 折线取所有端点的并集") {
    Scene scene;
    scene.push(MoveTo{Point{1.0, 2.0}});
    scene.push(LineTo{Point{5.0, 7.0}});
    scene.push(LineTo{Point{3.0, 0.5}});

    const Rect bounds = scene_bounds(scene);
    CHECK(bounds.x == doctest::Approx(1.0));
    CHECK(bounds.y == doctest::Approx(0.5));
    CHECK(bounds.w == doctest::Approx(4.0));  // x: 1 → 5
    CHECK(bounds.h == doctest::Approx(6.5));  // y: 0.5 → 7
}

TEST_CASE("scene/bounds: 矩形、圆弧、图片各按自己的几何计入") {
    Scene rect_scene;
    rect_scene.push(RectOp{Rect{0.0, 0.0, 4.0, 3.0}});
    const Rect rect_bounds = scene_bounds(rect_scene);
    CHECK(rect_bounds.w == doctest::Approx(4.0));
    CHECK(rect_bounds.h == doctest::Approx(3.0));

    Scene arc_scene;
    arc_scene.push(ArcOp{Point{10.0, 10.0}, 3.0, 0.0, 6.283185307179586});
    const Rect arc_bounds = scene_bounds(arc_scene);
    CHECK(arc_bounds.x == doctest::Approx(7.0));  // 圆心 ± 半径（保守：整圆方框）
    CHECK(arc_bounds.y == doctest::Approx(7.0));
    CHECK(arc_bounds.w == doctest::Approx(6.0));
    CHECK(arc_bounds.h == doctest::Approx(6.0));

    Scene image_scene;
    image_scene.push(ImageOp{Rect{2.0, 3.0, 5.0, 5.0}, 7});
    const Rect image_bounds = scene_bounds(image_scene);
    CHECK(image_bounds.x == doctest::Approx(2.0));
    CHECK(image_bounds.right() == doctest::Approx(7.0));
    CHECK(image_bounds.bottom() == doctest::Approx(8.0));
}

TEST_CASE("scene/bounds: 文本按显示宽度算，宽字符占两列") {
    Scene scene;
    scene.push(TextOp{Point{1.0, 1.0}, "abc", Style{}});
    const Rect ascii = scene_bounds(scene);
    CHECK(ascii.w == doctest::Approx(3.0));
    CHECK(ascii.h == doctest::Approx(1.0));

    Scene wide;
    wide.push(TextOp{Point{1.0, 1.0}, "中文", Style{}});
    CHECK(scene_bounds(wide).w == doctest::Approx(4.0));  // 两个宽字符 = 4 列
}

TEST_CASE("scene/bounds: 混合内容取并集，状态指令不改变结果") {
    Scene scene;
    scene.push(StrokeOp{Color::ansi16(3), 1.0});
    scene.push(MoveTo{Point{0.0, 0.0}});
    scene.push(LineTo{Point{2.0, 2.0}});
    scene.push(RectOp{Rect{10.0, 1.0, 3.0, 3.0}});
    scene.push(TextOp{Point{1.0, 5.0}, "hi", Style{}});

    const Rect bounds = scene_bounds(scene);
    CHECK(bounds.x == doctest::Approx(0.0));
    CHECK(bounds.y == doctest::Approx(0.0));
    CHECK(bounds.right() == doctest::Approx(13.0));  // 矩形右边界最远
    CHECK(bounds.bottom() == doctest::Approx(6.0));  // 文本那一行最靠下

    // 与"去掉状态指令"的版本必须一致
    Scene without_states;
    without_states.push(MoveTo{Point{0.0, 0.0}});
    without_states.push(LineTo{Point{2.0, 2.0}});
    without_states.push(RectOp{Rect{10.0, 1.0, 3.0, 3.0}});
    without_states.push(TextOp{Point{1.0, 5.0}, "hi", Style{}});
    const Rect same = scene_bounds(without_states);
    CHECK(same.x == bounds.x);
    CHECK(same.y == bounds.y);
    CHECK(same.w == bounds.w);
    CHECK(same.h == bounds.h);
}
