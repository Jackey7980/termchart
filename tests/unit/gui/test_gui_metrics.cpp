// test_gui_metrics.cpp —— GUI 平行栈：文本度量 / 滚轮与修饰键事件契约
//
// 关注的不是"能不能画一个框"，而是**口径一致**：
//   * text_size 与 draw_text 必须是同一把尺（显示宽度，CJK = 2 列）——
//     排版按它居中、绘制按它推进，两处偏一个像素控件就歪；
//   * Event 的 Wheel / modifiers 载荷完整（图表缩放与多选交互的地基）。
#include <doctest.h>

#include <string>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/native_loop/event.hpp"

namespace {

using namespace termchart_gui;

TEST_CASE("gui/metrics: text_size 与 draw_text 同一把尺") {
    BitmapCanvas canvas(64, 16);

    SUBCASE("ASCII：宽 = 字符数 × 6，高 = 8") {
        const Point s = canvas.text_size("abc");
        CHECK(s.x == 18.0);
        CHECK(s.y == 8.0);

        const Point empty = canvas.text_size("");
        CHECK(empty.x == 0.0);
        CHECK(empty.y == 8.0);
    }

    SUBCASE("CJK 按显示宽度 2 列（与 draw_text 推进一致）") {
        const Point cjk = canvas.text_size("图表");  // 2 码点 × 2 列 × 6
        CHECK(cjk.x == 24.0);
        CHECK(cjk.y == 8.0);

        // 一致性铁律：draw_text 返回的实际占用宽度必须等于 text_size 的宽度。
        termchart::image::Bitmap bmp(32, 16, 4);
        const double drawn = termchart::gui::draw_text(
            bmp, "图表", Point{0, 0}, Rgb{255, 255, 255}, 6.0, 8.0);
        CHECK(drawn == cjk.x);
    }

    SUBCASE("混排：ASCII 与 CJK 分别按 1/2 列计") {
        const Point mix = canvas.text_size("a图");  // 1 列 + 2 列 = 3 列
        CHECK(mix.x == 18.0);
    }
}

TEST_CASE("gui/event: Wheel 与 modifiers 载荷") {
    SUBCASE("Wheel 事件携带 delta 与修饰键") {
        Event e{};
        e.type = EventType::Wheel;
        e.x = 10.0;
        e.y = 20.0;
        e.wheel_delta = -1.0;  // 负 = 向下滚（缩小）
        e.modifiers = kModShift;
        CHECK(e.type == EventType::Wheel);
        CHECK(e.wheel_delta == -1.0);
        CHECK((e.modifiers & kModShift) != 0u);
        CHECK((e.modifiers & kModCtrl) == 0u);
    }

    SUBCASE("修饰键位可组合") {
        Event e{};
        e.modifiers = kModCtrl | kModAlt;
        CHECK((e.modifiers & kModCtrl) != 0u);
        CHECK((e.modifiers & kModAlt) != 0u);
        CHECK((e.modifiers & kModShift) == 0u);
    }

    SUBCASE("默认值不污染：非 wheel 事件 delta 为 0") {
        const Event press{};
        CHECK(press.wheel_delta == 0.0);
        CHECK(press.modifiers == 0u);
    }
}

TEST_CASE("gui/backend: 无头后端经队列派发 wheel 事件") {
    HeadlessBackend backend(32, 16);
    Event wheel{};
    wheel.type = EventType::Wheel;
    wheel.wheel_delta = 2.0;
    wheel.modifiers = kModCtrl;
    backend.inject_event(wheel);

    Event out{};
    REQUIRE(backend.poll_event(out));
    CHECK(out.type == EventType::Wheel);
    CHECK(out.wheel_delta == 2.0);
    CHECK((out.modifiers & kModCtrl) != 0u);
    CHECK_FALSE(backend.poll_event(out));  // 队列已空
}

}  // namespace
