// test_headless_pipeline.cpp —— 无头渲染端到端管线集成测试
//
// 覆盖 CLI/测试环境（无 tty）下 "Canvas 绘制 → CellBuffer → Renderer 字节流" 全链路：
// ① render_once 一次成帧；② HeadlessSession 增量帧；③ 帧属性可由 golden 解析器读回；
// ④ 颜色深度切换后仍产出合法字节流。不写 stdout，全程可在 CI 跑。
#include <doctest.h>

#include <string>

#include "golden/golden_runner.hpp"
#include "termchart/app/headless.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"

namespace {

using namespace termchart;

}  // namespace

TEST_CASE("integration/headless_pipeline: render_once 产出可解析的带色帧") {
    const auto draw = [](canvas::Canvas& c) {
        c.draw_text(Point{1.0, 0.0}, "HEADLESS",
                    Style{}.set_fg(Color::ansi16(2)).set_bg(Color::ansi16(0)));
        c.draw_line(Point{1.0, 1.0}, Point{12.0, 1.0}, U'-',
                    Style{}.set_fg(Color::ansi16(6)));
    };

    const auto frame = app::render_once(20, 4, draw);
    REQUIRE(frame.has_value());
    REQUIRE_FALSE(frame.value().empty());

    const golden::Props props = golden::parse_ansi(frame.value());
    CHECK(props.width == 20);
    CHECK(props.height >= 1);

    // 前景色确实进入字节流（有非默认 fg 的格）。
    bool saw_green = false;
    for (const auto& cell : props.cells) {
        if (cell.codepoint == U'H' && cell.fg == "ansi16:2") {
            saw_green = true;
            break;
        }
    }
    CHECK(saw_green);
}

TEST_CASE("integration/headless_pipeline: HeadlessSession 全量/增量/reset 三段一致") {
    app::HeadlessSession session(16, 3);

    session.canvas().draw_text(Point{0.0, 0.0}, "ab");
    const std::string full = session.frame_full();
    REQUIRE_FALSE(full.empty());

    // 全量首帧与 frame() 首帧属性一致（同一 prev 起点）。
    session.reset();
    const std::string first = session.frame();
    CHECK(golden::parse_ansi(first) == golden::parse_ansi(full));

    // 未变 → 增量空帧。
    CHECK(session.frame().empty());

    // 改一格 → 增量非空；全量帧解析后该格码点更新为 X。
    session.canvas().draw_text(Point{0.0, 0.0}, "XY");
    const std::string delta = session.frame();
    REQUIRE_FALSE(delta.empty());
    const golden::Props props = golden::parse_ansi(session.frame_full());
    bool saw_x = false;
    for (const auto& cell : props.cells) {
        if (cell.codepoint == U'X') {
            saw_x = true;
            break;
        }
    }
    CHECK(saw_x);
}

TEST_CASE("integration/headless_pipeline: 颜色深度切换后仍产出合法字节") {
    for (const ColorDepth depth : {ColorDepth::TrueColor, ColorDepth::Ansi256,
                                   ColorDepth::Ansi16, ColorDepth::None}) {
        const auto frame = app::render_once(
            10, 2,
            [](canvas::Canvas& c) {
                c.draw_text(Point{0.0, 0.0}, "D", Style{}.set_fg(Color::ansi16(4)));
            },
            depth);
        REQUIRE(frame.has_value());
        CHECK_FALSE(frame.value().empty());
        const golden::Props props = golden::parse_ansi(frame.value());
        bool saw_d = false;
        for (const auto& cell : props.cells) {
            if (cell.codepoint == U'D') {
                saw_d = true;
            }
        }
        CHECK(saw_d);
    }
}
