// test_interaction_immediate.cpp —— 即时模式绑定（平移/框选/滚轮缩放/非鼠标事件）
#include <doctest.h>

#include <cmath>

#include "termchart/interaction/event.hpp"
#include "termchart/interaction/immediate.hpp"
#include "termchart/interaction/key.hpp"
#include "termchart/interaction/mouse.hpp"
#include "termchart/interaction/select.hpp"
#include "termchart/interaction/zoom.hpp"

namespace {

using termchart::Point;
using termchart::Rect;
using termchart::interaction::BoxSelect;
using termchart::interaction::Event;
using termchart::interaction::ImmediateBindings;
using termchart::interaction::ImmediateContext;
using termchart::interaction::Key;
using termchart::interaction::KeyEvent;
using termchart::interaction::MouseAction;
using termchart::interaction::MouseButton;
using termchart::interaction::MouseEvent;
using termchart::interaction::PasteEvent;
using termchart::interaction::ResizeEvent;
using termchart::interaction::Viewport;
using termchart::interaction::ViewportOptions;
using termchart::interaction::FocusEvent;

// 左键平移、右键框选的常用配置。
ImmediateBindings pan_select_bindings() {
    return ImmediateBindings{1, 3, 1.25};
}

MouseEvent press(MouseButton button, int x, int y) {
    return MouseEvent{MouseAction::Press, button, x, y};
}

MouseEvent release(MouseButton button, int x, int y) {
    return MouseEvent{MouseAction::Release, button, x, y};
}

MouseEvent move_to(int x, int y) {
    return MouseEvent{MouseAction::Move, MouseButton::None, x, y};
}

// 可平移的视口：先放大一档，可见 500×500，四边都留有平移余量
// （visible 占满 data_bounds 时 pan_by 会被夹取钉死，测不出平移）。
Viewport pannable_viewport() {
    Viewport vp{ViewportOptions{Rect{0.0, 0.0, 1000.0, 1000.0}}};
    vp.zoom_at(2.0, Point{500.0, 500.0});
    return vp;
}

}  // namespace

TEST_CASE("interaction/immediate: 左键拖拽平移，方向为内容跟随指针") {
    ImmediateContext ctx{pan_select_bindings()};
    Viewport vp = pannable_viewport();
    BoxSelect sel;
    const Rect before = vp.visible();

    ctx.feed(Event{press(MouseButton::Left, 100, 100)}, vp, sel);
    CHECK(ctx.dragging());
    ctx.feed(Event{move_to(104, 102)}, vp, sel);  // 指针右移 4、下移 2

    const Rect after = vp.visible();
    // 内容跟随指针：视口向左/上移动同样的量。
    CHECK(after.x == doctest::Approx(before.x - 4.0));
    CHECK(after.y == doctest::Approx(before.y - 2.0));
    CHECK(after.w == doctest::Approx(before.w));

    ctx.feed(Event{release(MouseButton::Left, 104, 102)}, vp, sel);
    CHECK_FALSE(ctx.dragging());

    // 松开后移动不再平移。
    ctx.feed(Event{move_to(110, 110)}, vp, sel);
    CHECK(vp.visible().x == doctest::Approx(after.x));
    CHECK(vp.visible().y == doctest::Approx(after.y));
}

TEST_CASE("interaction/immediate: 右键框选三段产生 has_rect 且矩形归一化") {
    ImmediateContext ctx{pan_select_bindings()};
    Viewport vp = pannable_viewport();
    BoxSelect sel;
    const Rect initial = vp.visible();

    ctx.feed(Event{press(MouseButton::Right, 30, 25)}, vp, sel);
    ctx.feed(Event{move_to(20, 30)}, vp, sel);   // 向左下拖：仍归一化
    ctx.feed(Event{release(MouseButton::Right, 20, 30)}, vp, sel);

    CHECK_FALSE(ctx.dragging());
    CHECK(sel.has_rect());
    CHECK(sel.rect() == Rect{20.0, 25.0, 10.0, 5.0});

    // 框选不得顺带平移视口。
    CHECK(vp.visible() == initial);
}

TEST_CASE("interaction/immediate: 滚轮缩放改变 scale，锚点为鼠标位置") {
    ImmediateContext ctx{pan_select_bindings()};
    Viewport vp = pannable_viewport();
    BoxSelect sel;
    const Rect initial = vp.visible();

    // 先把鼠标移到 (200, 0)：缩放后视口原点应被该锚点"钉住"。
    ctx.feed(Event{move_to(200, 0)}, vp, sel);
    ctx.feed(Event{MouseEvent{MouseAction::ScrollUp, MouseButton::None, 200, 0}}, vp, sel);
    CHECK(ctx.mouse().x == 200);
    CHECK(vp.visible().w == doctest::Approx(initial.w / 1.25));
    CHECK(vp.scale_x() > doctest::Approx(1.0));

    // 反向滚动：放大跨度。
    const Rect zoomed = vp.visible();
    ctx.feed(Event{MouseEvent{MouseAction::ScrollDown, MouseButton::None, 200, 0}}, vp, sel);
    CHECK(vp.visible().w == doctest::Approx(zoomed.w * 1.25));
}

TEST_CASE("interaction/immediate: wheel_step 退化时滚轮不改状态") {
    ImmediateBindings bad;
    bad.wheel_step = 0.0;  // 1/step = Inf，两条方向都必须被拒绝
    ImmediateContext ctx{bad};
    Viewport vp = pannable_viewport();
    BoxSelect sel;
    const Rect before = vp.visible();

    ctx.feed(Event{MouseEvent{MouseAction::ScrollUp, MouseButton::None, 5, 5}}, vp, sel);
    ctx.feed(Event{MouseEvent{MouseAction::ScrollDown, MouseButton::None, 5, 5}}, vp, sel);
    CHECK(vp.visible() == before);

    ImmediateBindings nan_step;
    nan_step.wheel_step = std::numeric_limits<double>::quiet_NaN();
    ImmediateContext ctx2{nan_step};
    ctx2.feed(Event{MouseEvent{MouseAction::ScrollUp, MouseButton::None, 5, 5}}, vp, sel);
    CHECK(vp.visible() == before);
}

TEST_CASE("interaction/immediate: 非鼠标事件不影响任何状态") {
    ImmediateContext ctx{pan_select_bindings()};
    Viewport vp = pannable_viewport();
    BoxSelect sel;
    const Rect before = vp.visible();

    ctx.feed(Event{KeyEvent{Key::Char, U'+'}}, vp, sel);
    ctx.feed(Event{ResizeEvent{80, 24}}, vp, sel);
    ctx.feed(Event{FocusEvent{false}}, vp, sel);
    ctx.feed(Event{PasteEvent{"hello"}}, vp, sel);

    CHECK(ctx.mouse() == Point{0.0, 0.0});  // 从未收到过鼠标事件
    CHECK_FALSE(ctx.dragging());
    CHECK_FALSE(sel.active());
    CHECK_FALSE(sel.has_rect());
    CHECK(vp.visible() == before);
}

TEST_CASE("interaction/immediate: bindings 可查询、mouse 跟随最近事件") {
    ImmediateBindings cfg{2, 3, 2.0};  // 中键平移、右键框选
    ImmediateContext ctx{cfg};
    CHECK(ctx.bindings().pan_button == 2);
    CHECK(ctx.bindings().select_button == 3);
    CHECK(ctx.bindings().wheel_step == 2.0);

    Viewport vp = pannable_viewport();
    BoxSelect sel;
    ctx.feed(Event{MouseEvent{MouseAction::ScrollUp, MouseButton::None, 7, 9}}, vp, sel);
    CHECK(ctx.mouse() == Point{7.0, 9.0});
    ctx.feed(Event{move_to(11, 13)}, vp, sel);
    CHECK(ctx.mouse() == Point{11.0, 13.0});
}
