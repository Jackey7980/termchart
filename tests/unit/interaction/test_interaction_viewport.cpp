// test_interaction_viewport.cpp —— 视口缩放 zoom 与拖拽平移 pan
//
// 覆盖（对应任务清单的 6 条）：
//   1. 锚点不动性：多次 zoom_at 后 anchor 的归一化位置不变（含角/边/中心）。
//   2. 夹取：反复缩小不越 data_bounds；反复放大跨度不低于 min_span 且恒 > 0。
//   3. 平移夹取：pan_by 极大值后贴边且仍落在 data_bounds 内。
//   4. 往返与 reset：未触发夹取时 pan_by(+d) 再 pan_by(-d) 回原位；reset 严格等于 data_bounds。
//   5. 拖拽状态机：按下→多次移动（含反向、含未按下被忽略）→释放，累计位移等于各次之和。
//   6. 退化输入：data_bounds 零跨度 / factor <= 0 / NaN / Inf 明确不改状态。
#include <doctest.h>

#include <cmath>
#include <limits>
#include <optional>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/interaction/pan.hpp"
#include "termchart/interaction/zoom.hpp"

namespace {

using namespace termchart;
using namespace termchart::interaction;

// 锚点归一化容差来源：缩放是多次乘/除 1/factor 的浮点累积，量级 ~1e-12 足够严。
constexpr double kEps = 1e-12;

}  // namespace

TEST_CASE("interaction/zoom: 锚点在视口内的归一化位置保持不变") {
    const Rect data{0, 0, 10, 10};
    const Point anchors[] = {{5, 5}, {0, 0}, {10, 10}, {2.5, 7.5}, {0, 10}, {10, 0}};

    for (const Point& anchor : anchors) {
        CAPTURE(anchor.x);
        CAPTURE(anchor.y);
        Viewport vp(data);
        const double tx0 = vp.normalized_x(anchor.x);
        const double ty0 = vp.normalized_y(anchor.y);

        // 连续放大多次：跨度一路变小，全程不触发原点夹取，锚点应严格不动。
        for (int i = 0; i < 5; ++i) {
            vp.zoom_at(1.5, anchor);
        }
        CHECK(vp.normalized_x(anchor.x) == doctest::Approx(tx0).epsilon(kEps));
        CHECK(vp.normalized_y(anchor.y) == doctest::Approx(ty0).epsilon(kEps));
    }
}

TEST_CASE("interaction/zoom: 缩放/缩放的混合序列下锚点仍不动") {
    const Rect data{0, 0, 10, 10};
    const Point anchor{5.0, 5.0};
    Viewport vp(data);
    const double tx0 = vp.normalized_x(anchor.x);
    const double ty0 = vp.normalized_y(anchor.y);

    // 进→退→进：只要不夹取，锚点性质与操作次数无关。
    vp.zoom_at(2.0, anchor);   // 放大
    vp.zoom_at(1.25, anchor);  // 再放大
    vp.zoom_at(0.8, anchor);   // 缩小
    vp.zoom_at(1.5, anchor);   // 放大

    CHECK(vp.normalized_x(anchor.x) == doctest::Approx(tx0).epsilon(kEps));
    CHECK(vp.normalized_y(anchor.y) == doctest::Approx(ty0).epsilon(kEps));
}

TEST_CASE("interaction/zoom: 缩小不越界、放大不下溢") {
    const Rect data{0, 0, 10, 10};

    // 反复缩小到极端：可见矩形最终占满数据界，绝不超过。
    Viewport out(data);
    for (int i = 0; i < 200; ++i) {
        out.zoom_at(0.5, Point{5, 5});
    }
    const Rect v_out = out.visible();
    CHECK(v_out.x >= data.x - kEps);
    CHECK(v_out.y >= data.y - kEps);
    CHECK(v_out.right() <= data.right() + kEps);
    CHECK(v_out.bottom() <= data.bottom() + kEps);
    CHECK(v_out.w == doctest::Approx(data.w));
    CHECK(v_out.h == doctest::Approx(data.h));

    // 反复放大到极端：跨度被 min_span 兜住，恒 > 0，且不越界。
    Viewport in(data);
    for (int i = 0; i < 400; ++i) {
        in.zoom_at(2.0, Point{5, 5});
    }
    const Rect v_in = in.visible();
    CHECK(v_in.w > 0.0);
    CHECK(v_in.h > 0.0);
    CHECK(v_in.w == doctest::Approx(kDefaultMinSpan));
    CHECK(v_in.h == doctest::Approx(kDefaultMinSpan));
    CHECK(v_in.x >= data.x - kEps);
    CHECK(v_in.right() <= data.right() + kEps);
    // 放大不下溢：scale 为正且有限。
    CHECK(in.scale_x() > 0.0);
    CHECK(std::isfinite(in.scale_x()));
}

TEST_CASE("interaction/zoom: keep_aspect 保持可见矩形宽高比") {
    // 数据界 10:10，保持比例即缩放后仍是正方形。
    Viewport vp(ViewportOptions{Rect{0, 0, 10, 10}, kDefaultMinSpan, true});
    const double ratio0 = vp.visible().w / vp.visible().h;

    vp.zoom_at(2.0, Point{5, 5});  // 放大
    CHECK(vp.visible().w / vp.visible().h == doctest::Approx(ratio0).epsilon(kEps));

    vp.zoom_at(0.25, Point{3, 3});  // 缩小（很可能触界，夹取仍应保比例）
    CHECK(vp.visible().w / vp.visible().h == doctest::Approx(ratio0).epsilon(kEps));

    // 非 1:1 的数据界同样保持其自身比例。
    Viewport wide(ViewportOptions{Rect{0, 0, 20, 10}, kDefaultMinSpan, true});
    const double ratio_w = wide.visible().w / wide.visible().h;  // = 2
    wide.zoom_at(2.0, Point{10, 5});
    CHECK(wide.visible().w / wide.visible().h == doctest::Approx(ratio_w).epsilon(kEps));
}

TEST_CASE("interaction/zoom: scale 与归一化映射") {
    Viewport vp(Rect{0, 0, 10, 20});
    CHECK(vp.scale_x() == doctest::Approx(1.0));
    CHECK(vp.scale_y() == doctest::Approx(1.0));
    CHECK(vp.normalized_x(0.0) == doctest::Approx(0.0));
    CHECK(vp.normalized_x(10.0) == doctest::Approx(1.0));
    CHECK(vp.normalized_y(20.0) == doctest::Approx(1.0));

    vp.zoom_at(2.0, Point{5, 10});  // 以中心放大 2 倍
    CHECK(vp.scale_x() == doctest::Approx(2.0));
    CHECK(vp.scale_y() == doctest::Approx(2.0));
    CHECK(vp.normalized_x(5.0) == doctest::Approx(0.5));
    CHECK(vp.normalized_y(10.0) == doctest::Approx(0.5));
}

TEST_CASE("interaction/pan: 平移夹取贴边") {
    Viewport vp(Rect{0, 0, 10, 10});
    vp.zoom_at(2.0, Point{5, 5});  // 可见 5x5 居中 → {2.5, 2.5, 5, 5}
    REQUIRE(vp.visible().w == doctest::Approx(5.0));

    vp.pan_by(1e9, 0);  // 向右猛推 → 贴右缘 data.right - w = 5
    CHECK(vp.visible().x == doctest::Approx(5.0));
    CHECK(vp.visible().right() == doctest::Approx(10.0));

    vp.pan_by(-1e9, 0);  // 反向猛推 → 贴左缘 0
    CHECK(vp.visible().x == doctest::Approx(0.0));
    CHECK(vp.visible().right() == doctest::Approx(5.0));

    vp.pan_by(0, 1e9);  // 向下猛推 → 贴下缘 5
    CHECK(vp.visible().y == doctest::Approx(5.0));
    CHECK(vp.visible().bottom() == doctest::Approx(10.0));

    vp.pan_by(0, -1e9);  // 向上猛推 → 贴上缘 0
    CHECK(vp.visible().y == doctest::Approx(0.0));
    CHECK(vp.visible().bottom() == doctest::Approx(5.0));
}

TEST_CASE("interaction/pan: 往返与 reset") {
    Viewport vp(Rect{0, 0, 10, 10});
    vp.zoom_at(2.0, Point{5, 5});  // {2.5, 2.5, 5, 5}
    const Rect start = vp.visible();

    // 未触发夹取的小幅往返：严格回到原位。
    vp.pan_by(1.0, -0.5);
    vp.pan_by(-1.0, 0.5);
    CHECK(vp.visible().x == doctest::Approx(start.x));
    CHECK(vp.visible().y == doctest::Approx(start.y));
    CHECK(vp.visible().w == doctest::Approx(start.w));
    CHECK(vp.visible().h == doctest::Approx(start.h));

    // reset：严格等于 data_bounds。
    vp.reset();
    CHECK(vp.visible() == Rect{0, 0, 10, 10});
    CHECK(vp.scale_x() == doctest::Approx(1.0));
    CHECK(vp.scale_y() == doctest::Approx(1.0));
}

TEST_CASE("interaction/pan: 拖拽状态机 按下→移动→释放") {
    const Rect data{0, 0, 10, 10};
    Viewport vp(data);
    vp.zoom_at(2.0, Point{5, 5});  // {2.5, 2.5, 5, 5}
    const Rect start = vp.visible();

    PanGesture g;
    CHECK_FALSE(g.dragging());

    // 未按下：移动必须被忽略（返回 nullopt），视口纹丝不动。
    CHECK_FALSE(g.drag_to(Point{100, 100}).has_value());
    CHECK(vp.visible() == start);

    // 按下，随后多次移动（含反向）。
    g.press(Point{0, 0});
    CHECK(g.dragging());

    const std::optional<Point> d1 = g.drag_to(Point{1, 2});
    REQUIRE(d1.has_value());
    CHECK(d1->x == doctest::Approx(1.0));
    CHECK(d1->y == doctest::Approx(2.0));
    vp.pan_by(-d1->x, -d1->y);  // 内容跟随指针：视口取反方向

    const std::optional<Point> d2 = g.drag_to(Point{0, 1});  // 反向
    REQUIRE(d2.has_value());
    CHECK(d2->x == doctest::Approx(-1.0));
    CHECK(d2->y == doctest::Approx(-1.0));
    vp.pan_by(-d2->x, -d2->y);

    // 累计指针位移 = 各次增量之和 = last - origin。
    CHECK(g.pointer_displacement().x == doctest::Approx(0.0));
    CHECK(g.pointer_displacement().y == doctest::Approx(1.0));
    CHECK(g.pointer_displacement().x == doctest::Approx(d1->x + d2->x));
    CHECK(g.pointer_displacement().y == doctest::Approx(d1->y + d2->y));

    // 视口净位移 = -累计指针位移 = (0,-1)，落在未触夹取区间内。
    CHECK(vp.visible().x == doctest::Approx(start.x));
    CHECK(vp.visible().y == doctest::Approx(start.y - 1.0));
    CHECK(vp.visible().w == doctest::Approx(start.w));
    CHECK(vp.visible().h == doctest::Approx(start.h));

    // 释放后：移动再次被忽略。
    g.release();
    CHECK_FALSE(g.dragging());
    const Rect after = vp.visible();
    CHECK_FALSE(g.drag_to(Point{9, 9}).has_value());
    CHECK(vp.visible() == after);

    // reset 清空记录但不加视口。
    g.reset();
    CHECK(g.origin() == Point{});
    CHECK(g.last() == Point{});
    CHECK(g.pointer_displacement() == Point{});
}

TEST_CASE("interaction/pan: drag_into 按自然方向驱动视口") {
    Viewport vp(Rect{0, 0, 10, 10});
    vp.zoom_at(2.0, Point{5, 5});  // {2.5, 2.5, 5, 5}

    PanGesture g;
    // 未按下：返回 false 且不动视口。
    CHECK_FALSE(g.drag_into(Point{1, 1}, vp));
    CHECK(vp.visible() == Rect{2.5, 2.5, 5, 5});

    g.press(Point{0, 0});
    // 指针向右下拖 → 内容跟随 → 视口向左上移动。
    CHECK(g.drag_into(Point{1, 2}, vp));
    CHECK(vp.visible().x == doctest::Approx(1.5));
    CHECK(vp.visible().y == doctest::Approx(0.5));
}

TEST_CASE("interaction/zoom: 退化输入明确不改状态") {
    // (a) data_bounds 宽为 0 → 空矩形 → zoom/pan 均 no-op。
    Viewport zero_w(Rect{0, 0, 0, 10});
    const Rect before_w = zero_w.visible();
    zero_w.zoom_at(2.0, Point{0, 5});
    zero_w.pan_by(100, 100);
    CHECK(zero_w.visible() == before_w);

    // (b) data_bounds 高为 0 → 同样 no-op。
    Viewport zero_h(Rect{0, 0, 10, 0});
    const Rect before_h = zero_h.visible();
    zero_h.zoom_at(0.5, Point{5, 0});
    zero_h.pan_by(-100, -100);
    CHECK(zero_h.visible() == before_h);

    // (c) factor <= 0 / NaN / Inf：先真的放大一次，之后非法因子必须不改状态。
    Viewport vp(Rect{0, 0, 10, 10});
    vp.zoom_at(3.0, Point{5, 5});
    const Rect zoomed = vp.visible();
    vp.zoom_at(0.0, Point{5, 5});
    vp.zoom_at(-2.0, Point{5, 5});
    vp.zoom_at(std::nan(""), Point{5, 5});
    vp.zoom_at(std::numeric_limits<double>::infinity(), Point{5, 5});
    CHECK(vp.visible() == zoomed);

    // (d) 非法平移量（NaN/Inf）不改状态。
    vp.pan_by(std::nan(""), 0.0);
    vp.pan_by(0.0, std::numeric_limits<double>::infinity());
    CHECK(vp.visible() == zoomed);
}
