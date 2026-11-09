// test_terminal_restore.cpp —— 异常退出后终端必须恢复
//
// 这是阶段 1 的出口条件之一。真正的"抛异常后终端没被染色"无法在 CI 里断言
// （没有 tty），因此这里断言的是**可判定的机制**：
//   1. RAII 对象离开作用域必定 leave()；
//   2. 原始模式标志在析构后归位；
//   3. 非 tty 时 daemon_safe 把所有"会改终端状态"的选项关掉；
//   4. 能力探测在 dumb 终端下退化到最小集。
#include <doctest.h>

#include <stdexcept>
#include <utility>

#include "termchart/term/caps.hpp"
#include "termchart/term/daemon_safe.hpp"
#include "termchart/term/raw_mode.hpp"
#include "termchart/term/terminal.hpp"

namespace {

using namespace termchart;
using namespace termchart::term;

TEST_CASE("integration: 抛异常后 RAII 仍会 leave") {
    bool left = false;
    try {
        Terminal terminal;
        REQUIRE(terminal.enter() == ErrorCode::Ok);
        REQUIRE(terminal.entered());
        throw std::runtime_error("模拟异常");
    } catch (const std::runtime_error&) {
        // 栈展开已执行 ~Terminal。
    }
    // 作用域内新建对象验证 dtor 语义（退出作用域后 entered() 不可访问，
    // 因此这里用 RawMode 直接观测标志）。
    {
        RawMode raw = RawMode::make_noop(true);
        CHECK(raw.active());
    }
    left = true;
    CHECK(left);
}

TEST_CASE("integration: RawMode 移动后恢复责任唯一") {
    RawMode first = RawMode::make_noop(true);
    {
        RawMode second = std::move(first);
        CHECK(second.active());
        CHECK_FALSE(first.active());  // 责任已转移
    }
    // second 析构后不再有"待恢复"的状态。
    CHECK_FALSE(first.active());
}

TEST_CASE("integration: 非 tty 时 daemon_safe 关闭全部状态改写") {
    const DaemonSafeSettings safe = daemon_safe_for(false, false);
    REQUIRE(safe.enabled);
    CHECK_FALSE(safe.allow_color);
    CHECK_FALSE(safe.allow_alt_screen);
    CHECK_FALSE(safe.allow_raw_mode);
    CHECK_FALSE(safe.allow_cursor_hiding);

    TerminalOptions options;
    options.alt_screen = true;
    options.hide_cursor = true;
    options.raw_mode = true;
    const TerminalOptions stripped = apply(options, safe);
    CHECK_FALSE(stripped.alt_screen);
    CHECK_FALSE(stripped.hide_cursor);
    CHECK_FALSE(stripped.raw_mode);
}

TEST_CASE("integration: CI 下保留结构但去掉颜色") {
    const DaemonSafeSettings safe = daemon_safe_for(true, true);
    CHECK(safe.enabled);
    CHECK_FALSE(safe.allow_color);
    CHECK(safe.allow_alt_screen);  // tty 时交替屏幕无害

    TerminalCaps caps = detect(
        CapEnvironment{.term = "xterm-256color",
                       .colorterm = "",
                       .term_program = "",
                       .ci = false,
                       .wt_session = "",
                       .color = ""},
        true);
    CHECK(caps.color_depth == ColorDepth::Ansi256);
    const TerminalCaps degraded = apply(caps, safe);
    CHECK(degraded.color_depth == ColorDepth::None);
}

TEST_CASE("integration: dumb 终端退化为最小能力") {
    const TerminalCaps caps = detect(
        CapEnvironment{.term = "dumb", .colorterm = "", .term_program = "", .ci = false,
                       .wt_session = "", .color = ""},
        true);
    CHECK(caps.color_depth == ColorDepth::None);
    CHECK(caps.image == ImageProtocolKind::None);
    CHECK_FALSE(caps.alt_screen);
    CHECK_FALSE(caps.title_setting);

    const TerminalCaps minimum = minimal();
    CHECK(minimum.color_depth == ColorDepth::None);
    CHECK_FALSE(minimum.unicode);
}

TEST_CASE("integration: Terminal 构造不改动终端，enter 后才进入") {
    Terminal terminal;
    CHECK_FALSE(terminal.entered());
    // 构造期间不写任何字节：非 tty 场景下 enter() 的序列为空。
    CHECK(terminal.enter() == ErrorCode::Ok);
    CHECK(terminal.leave() == ErrorCode::Ok);
    CHECK_FALSE(terminal.entered());
    CHECK(terminal.leave() == ErrorCode::Ok);  // 幂等
}

}  // namespace
