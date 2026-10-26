// test_interaction_data_zoom.cpp —— DataZoom 范围滑块（纯几何）
//
// 覆盖：
//   1. 坐标往返：value_at(screen_pos_of(v)) == v（轨道内）。
//   2. 命中：左/右手柄、窗体 Body、轨道外 None、带宽外 None。
//   3. 拖左/右端点：窗口收缩到最小跨度、贴数据界、不越过对侧。
//   4. 拖窗体 Body：宽度守恒、整体平移夹进数据界。
//   5. 退化：数据界零跨度 pin 成一点；set_window 越界/倒序被夹正。
//   6. 归一化端点 frac 与 reset。
#include <doctest.h>

#include <cmath>
#include <limits>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/interaction/data_zoom.hpp"

namespace {

using namespace termchart;
using namespace termchart::interaction;
using doctest::Approx;

constexpr double kEps = 1e-9;

}  // namespace

TEST_CASE("data_zoom: 屏幕↔数据坐标往返一致") {
    const Rect track{0, 0, 100, 1};  // 横向轨道，长度 100
    DataZoom dz(-10.0, 10.0, -2.0, 4.0, track, ZoomAxis::Horizontal);
    for (double v = -10.0; v <= 10.0; v += 2.5) {
        CHECK(dz.value_at(dz.screen_pos_of(v)) == Approx(v).epsilon(kEps));
    }
}

TEST_CASE("data_zoom: 竖向轨道用 y 方向折算") {
    const Rect track{0, 0, 1, 50};  // 竖向轨道，长度 50
    DataZoom dz(0.0, 1.0, 0.2, 0.8, track, ZoomAxis::Vertical);
    CHECK(dz.screen_pos_of(0.5) == Approx(25.0).epsilon(kEps));
    CHECK(dz.value_at(25.0) == Approx(0.5).epsilon(kEps));
}

TEST_CASE("data_zoom: 命中分区正确") {
    const Rect track{0, 0, 100, 10};
    DataZoom dz(0.0, 100.0, 20.0, 80.0, track, ZoomAxis::Horizontal);
    // 端点手柄（屏幕 20 与 80 附近）在带宽 0..10 内。
    CHECK(dz.hit_test(20.0, 5.0) == DataZoom::Grab::Left);
    CHECK(dz.hit_test(80.0, 5.0) == DataZoom::Grab::Right);
    // 窗体内（屏幕 50）。
    CHECK(dz.hit_test(50.0, 5.0) == DataZoom::Grab::Body);
    // 窗体外（屏幕 10 不在窗体 20..80）。
    CHECK(dz.hit_test(10.0, 5.0) == DataZoom::Grab::None);
    // 带宽外（screen_other 不在 0..10）。
    CHECK(dz.hit_test(50.0, 15.0) == DataZoom::Grab::None);
}

TEST_CASE("data_zoom: 拖右端点越过右数据界被夹住") {
    const Rect track{0, 0, 100, 1};
    DataZoom dz(0.0, 100.0, 20.0, 80.0, track, ZoomAxis::Horizontal);
    dz.begin(80.0);
    REQUIRE(dz.dragging());
    dz.move(150.0);  // 越过右界 → 右沿夹到 data_max，左沿不动。
    CHECK(dz.window_max() == Approx(100.0).epsilon(kEps));
    CHECK(dz.window_min() == Approx(20.0).epsilon(kEps));
    dz.end();
    CHECK_FALSE(dz.dragging());
}

TEST_CASE("data_zoom: 拖左端点越过左数据界被夹住") {
    const Rect track{0, 0, 100, 1};
    DataZoom dz(0.0, 100.0, 20.0, 80.0, track, ZoomAxis::Horizontal);
    dz.begin(20.0);  // Left
    REQUIRE(dz.dragging());
    dz.move(-50.0);  // 越过左界 → 左沿夹到 data_min，右沿不动。
    CHECK(dz.window_min() == Approx(0.0).epsilon(kEps));
    CHECK(dz.window_max() == Approx(80.0).epsilon(kEps));
    dz.end();
}

TEST_CASE("data_zoom: 右端点不能越过左端点（最小跨度约束）") {
    const Rect track{0, 0, 100, 1};
    DataZoom dz(0.0, 100.0, 20.0, 80.0, track, ZoomAxis::Horizontal,
                DataZoomOptions{/*min_window=*/5.0});
    dz.begin(80.0);
    REQUIRE(dz.dragging());
    dz.move(22.0);  // 右沿拉到接近左沿，应被最小跨度 5 顶住。
    CHECK(dz.window_max() == Approx(25.0).epsilon(kEps));
    CHECK(dz.window_min() == Approx(20.0).epsilon(kEps));
    dz.end();
}

TEST_CASE("data_zoom: 拖窗体保持宽度并夹进数据界") {
    const Rect track{0, 0, 100, 1};
    DataZoom dz(0.0, 100.0, 40.0, 60.0, track, ZoomAxis::Horizontal);  // 宽 20
    dz.begin(50.0);  // Body
    REQUIRE(dz.dragging());
    dz.move(90.0);  // 整体右移，宽度 20 守恒，贴右界
    CHECK(dz.window_max() == Approx(100.0).epsilon(kEps));
    CHECK(dz.window_min() == Approx(80.0).epsilon(kEps));
    dz.move(-100.0);  // 整体左移，贴左界
    CHECK(dz.window_min() == Approx(0.0).epsilon(kEps));
    CHECK(dz.window_max() == Approx(20.0).epsilon(kEps));
}

TEST_CASE("data_zoom: 未 begin 的 move 不改窗口") {
    const Rect track{0, 0, 100, 1};
    DataZoom dz(0.0, 100.0, 20.0, 80.0, track, ZoomAxis::Horizontal);
    dz.move(50.0);  // 未 begin → no-op
    CHECK(dz.window_min() == Approx(20.0).epsilon(kEps));
    CHECK(dz.window_max() == Approx(80.0).epsilon(kEps));
}

TEST_CASE("data_zoom: 数据界退化 pin 成一点") {
    const Rect track{0, 0, 100, 1};
    DataZoom dz(5.0, 5.0, 1.0, 9.0, track, ZoomAxis::Horizontal);
    CHECK(dz.window_min() == Approx(5.0).epsilon(kEps));
    CHECK(dz.window_max() == Approx(5.0).epsilon(kEps));
    dz.set_window(2.0, 8.0);  // 倒序+越界，仍被夹成正点
    CHECK(dz.window_min() == Approx(5.0).epsilon(kEps));
    CHECK(dz.window_max() == Approx(5.0).epsilon(kEps));
}

TEST_CASE("data_zoom: 归一化端点与 reset") {
    const Rect track{0, 0, 100, 1};
    DataZoom dz(0.0, 100.0, 25.0, 75.0, track, ZoomAxis::Horizontal);
    CHECK(dz.window_lo_frac() == Approx(0.25).epsilon(kEps));
    CHECK(dz.window_hi_frac() == Approx(0.75).epsilon(kEps));
    dz.set_window(10.0, 20.0);
    dz.reset();
    CHECK(dz.window_min() == Approx(0.0).epsilon(kEps));
    CHECK(dz.window_max() == Approx(100.0).epsilon(kEps));
}
