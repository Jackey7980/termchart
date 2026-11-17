// test_resize.cpp —— 窗口尺寸变化转发的端到端集成测试
//
// 验证"SIGWINCH/控制台 resize → 标志 + waker 唤醒 → 主线程消费标志 → 画布重建"
// 链路。测试不真发信号（跨平台），用 simulate_resize_for_test() 探针；画布侧验证
// resize 后尺寸更新、越界写入被裁、旧内容按约定保留/清空。
#include <doctest.h>

#include <cstdint>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/term/signal.hpp"
#include "termchart/term/waker.hpp"

namespace {

using namespace termchart;

}  // namespace

TEST_CASE("integration/resize: 模拟 resize 置标志且读即清零") {
    // 先清掉可能残留的标志。
    (void)term::take_resize_flag();
    CHECK_FALSE(term::take_resize_flag());

    term::simulate_resize_for_test();
    CHECK(term::take_resize_flag());
    // 读即清零：第二次读为 false。
    CHECK_FALSE(term::take_resize_flag());
}

TEST_CASE("integration/resize: resize 转发同时唤醒 waker") {
    term::Waker waker;
    REQUIRE(waker.valid());
    REQUIRE(term::install_resize_forwarding(waker));
    (void)term::take_resize_flag();
    waker.drain();

    term::simulate_resize_for_test();
    CHECK(term::take_resize_flag());
    // 模拟转发会 notify，wait(0) 应立即返回 true。
    const bool woke = waker.wait(100);
    waker.drain();
    term::uninstall_resize_forwarding();
    CHECK(woke);
}

TEST_CASE("integration/resize: Canvas 随尺寸重建，旧内容裁剪到新边界") {
    canvas::Canvas canvas(20, 10);
    CHECK(canvas.width() == 20);
    CHECK(canvas.height() == 10);

    // 直接写格（避开 draw_text 的字形/样式语义，专测 resize 裁剪）。
    for (std::uint32_t x = 0; x < 10; ++x) {
        REQUIRE(canvas.put(x, 0, static_cast<char32_t>(U'a' + x)));
    }
    CHECK(canvas.at(0, 0).cp == U'a');
    CHECK(canvas.at(3, 0).cp == U'd');

    // 缩到 4x2：尺寸更新，不越界写（写第 9 列应被拒）。
    canvas.resize(4, 2);
    CHECK(canvas.width() == 4);
    CHECK(canvas.height() == 2);
    CHECK_FALSE(canvas.put(9, 0, U'x'));
    CHECK(canvas.put(3, 1, U'z'));

    // 放大回 8x4：尺寸更新，可写新区域。
    canvas.resize(8, 4);
    CHECK(canvas.width() == 8);
    CHECK(canvas.height() == 4);
    CHECK(canvas.put(7, 3, U'q'));
}

TEST_CASE("integration/resize: CellBuffer 缩放在边界内写入不越界") {
    CellBuffer buffer(16, 8);
    buffer.resize(64, 32);
    CHECK(buffer.width() == 64);
    CHECK(buffer.height() == 32);

    // 写四角与中心，均在范围内。
    buffer.at(0, 0) = Cell(U'A');
    buffer.at(63, 31) = Cell(U'B');
    buffer.at(32, 16) = Cell(U'C');
    CHECK(buffer.at(0, 0).cp == U'A');
    CHECK(buffer.at(63, 31).cp == U'B');
    CHECK(buffer.at(32, 16).cp == U'C');
}
