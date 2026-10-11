// test_term.cpp —— 终端能力探测 / 非 tty 安全 / ANSI 控制序列
//
// 这一层的价值在于**纯函数化**：detect / daemon_safe_for / apply 都不读写终端，
// 输入是环境变量快照 + 是否 tty，于是 CI 与交互终端里都能复现。
// GUI 平行栈将来只复用 scene 及以下，但"自己是什么终端"这类判断仍走这里。
#include <doctest.h>

#include <string>
#include <string_view>

#include "termchart/core/color_depth.hpp"
#include "termchart/term/ansi.hpp"
#include "termchart/term/caps.hpp"
#include "termchart/term/daemon_safe.hpp"
#include "termchart/term/terminal.hpp"

namespace {

using namespace termchart;
using namespace termchart::term;

CapEnvironment env_of(std::string_view term, std::string_view colorterm = {},
                      std::string_view term_program = {}, bool ci = false,
                      std::string_view wt_session = {}) noexcept {
    CapEnvironment env;
    env.term = term;
    env.colorterm = colorterm;
    env.term_program = term_program;
    env.ci = ci;
    env.wt_session = wt_session;
    return env;
}

std::string esc(const char* tail) { return std::string("\x1b[") + tail; }

}  // namespace

TEST_CASE("term: 颜色档位优先级 COLORTERM > WT_SESSION > TERM_PROGRAM > TERM > 默认") {
    CHECK(color_depth_for(env_of("xterm", "truecolor")) == ColorDepth::TrueColor);
    CHECK(color_depth_for(env_of("xterm", "24bit")) == ColorDepth::TrueColor);
    // Windows Terminal：WT_SESSION 是它的指纹，此时 TERM/COLORTERM/TERM_PROGRAM 全空。
    CHECK(color_depth_for(env_of("", "", "", false, "wt-session-guid")) ==
          ColorDepth::TrueColor);
    CHECK(color_depth_for(env_of("xterm", "", "iTerm.app")) == ColorDepth::TrueColor);
    CHECK(color_depth_for(env_of("xterm", "", "WezTerm")) == ColorDepth::TrueColor);
    CHECK(color_depth_for(env_of("screen-256color")) == ColorDepth::Ansi256);
    CHECK(color_depth_for(env_of("dumb")) == ColorDepth::None);
    // TERM 为空 ≠ 哑终端：Windows 原生会话里 TERM 为空是常态，但支持颜色。
    CHECK(color_depth_for(env_of("")) == ColorDepth::Ansi16);
    CHECK(color_depth_for(env_of("linux")) == ColorDepth::Ansi16);  // 默认保守到 16 色
}

TEST_CASE("term: 图像协议按 TERM_PROGRAM / TERM 推断，底线是半块") {
    CHECK(image_protocol_for(env_of("xterm", "", "iTerm.app")) == ImageProtocolKind::ITerm2);
    CHECK(image_protocol_for(env_of("xterm-kitty")) == ImageProtocolKind::Kitty);
    CHECK(image_protocol_for(env_of("foot")) == ImageProtocolKind::Sixel);
    CHECK(image_protocol_for(env_of("vt100")) == ImageProtocolKind::Halfblock);
}

TEST_CASE("term: 协议名是稳定标识") {
    CHECK(std::string(image_protocol_name(ImageProtocolKind::None)) == "none");
    CHECK(std::string(image_protocol_name(ImageProtocolKind::Halfblock)) == "halfblock");
    CHECK(std::string(image_protocol_name(ImageProtocolKind::Sixel)) == "sixel");
    CHECK(std::string(image_protocol_name(ImageProtocolKind::Kitty)) == "kitty");
    CHECK(std::string(image_protocol_name(ImageProtocolKind::ITerm2)) == "iterm2");
}

TEST_CASE("term: dumb 与 linux 不发 Unicode") {
    CHECK_FALSE(unicode_for(env_of("dumb")));
    CHECK_FALSE(unicode_for(env_of("linux")));
    CHECK(unicode_for(env_of("xterm-256color")));
}

TEST_CASE("term: detect 把 tty / CI 折进能力") {
    const TerminalCaps interactive = detect(env_of("xterm-256color"), true);
    CHECK(interactive.mouse);
    CHECK(interactive.alt_screen);
    CHECK(interactive.title_setting);

    const TerminalCaps piped = detect(env_of("xterm-256color"), false);
    CHECK_FALSE(piped.mouse);
    CHECK_FALSE(piped.alt_screen);
    CHECK_FALSE(piped.title_setting);
    CHECK(piped.color_depth == ColorDepth::Ansi256);  // 颜色档位不看 tty

    const TerminalCaps in_ci = detect(env_of("xterm-256color", "", "", true), true);
    CHECK_FALSE(in_ci.mouse);
    CHECK(in_ci.alt_screen);
}

TEST_CASE("term: dumb 终端整体退化") {
    const TerminalCaps caps = detect(env_of("dumb"), true);
    CHECK(caps.color_depth == ColorDepth::None);
    CHECK(caps.image == ImageProtocolKind::None);
    CHECK_FALSE(caps.alt_screen);
    CHECK_FALSE(caps.title_setting);
}

TEST_CASE("term: minimal 是最保守能力") {
    const TerminalCaps caps = minimal();
    CHECK_FALSE(caps.unicode);
    CHECK(caps.color_depth == ColorDepth::None);
    CHECK(caps.image == ImageProtocolKind::None);
    CHECK_FALSE(caps.mouse);
    CHECK_FALSE(caps.alt_screen);
    CHECK_FALSE(caps.title_setting);
}

TEST_CASE("term: daemon_safe 在非 tty 关掉一切状态改写") {
    const DaemonSafeSettings piped = daemon_safe_for(false, false);
    CHECK(piped.enabled);
    CHECK_FALSE(piped.allow_color);
    CHECK_FALSE(piped.allow_alt_screen);
    CHECK_FALSE(piped.allow_raw_mode);
    CHECK_FALSE(piped.allow_cursor_hiding);

    const DaemonSafeSettings ci_tty = daemon_safe_for(true, true);
    CHECK(ci_tty.enabled);
    CHECK_FALSE(ci_tty.allow_color);
    CHECK(ci_tty.allow_alt_screen);  // CI 里结构保留，只去颜色

    const DaemonSafeSettings normal = daemon_safe_for(true, false);
    CHECK_FALSE(normal.enabled);
    CHECK(normal.allow_color);
    CHECK(normal.allow_alt_screen);
}

TEST_CASE("term: apply 只降级不报错，且不动入参") {
    const TerminalCaps caps = detect(env_of("xterm-256color"), true);
    const TerminalCaps degraded = apply(caps, daemon_safe_for(false, false));
    CHECK(degraded.color_depth == ColorDepth::None);
    CHECK_FALSE(degraded.alt_screen);
    CHECK(caps.color_depth == ColorDepth::Ansi256);  // 入参未被改动

    TerminalOptions options;  // 默认开 alt_screen / hide_cursor
    options.raw_mode = true;
    options.enable_mouse = true;
    const TerminalOptions safe = apply(options, daemon_safe_for(false, false));
    CHECK_FALSE(safe.alt_screen);
    CHECK_FALSE(safe.hide_cursor);
    CHECK_FALSE(safe.raw_mode);
    CHECK_FALSE(safe.enable_mouse);
    CHECK(options.alt_screen);
    CHECK(options.hide_cursor);
}

TEST_CASE("term: ANSI 控制序列格式固定") {
    CHECK(ansi::cursor_to(1, 1) == esc("1;1H"));
    CHECK(ansi::cursor_to(7, 3) == esc("7;3H"));
    CHECK(ansi::cursor_home() == esc("H"));
    CHECK(ansi::erase_line() == esc("2K"));
    CHECK(ansi::hide_cursor() == esc("?25l"));
    CHECK(ansi::show_cursor() == esc("?25h"));
    CHECK(ansi::enter_alt_screen() == esc("?1049h"));
    CHECK(ansi::exit_alt_screen() == esc("?1049l"));
}

TEST_CASE("term: append_move 永远选最短序列") {
    std::string out;
    ansi::append_move(out, 1, 1, 1, 1);
    CHECK(out.empty());  // 原地不动：零字节

    out.clear();
    ansi::append_move(out, 1, 1, 1, 4);
    CHECK(out == esc("3C"));

    out.clear();
    ansi::append_move(out, 1, 9, 1, 5);
    CHECK(out == esc("4D"));

    out.clear();
    ansi::append_move(out, 1, 1, 3, 1);
    CHECK(out == std::string(esc("2B")) + "\r");

    out.clear();
    ansi::append_move(out, 5, 1, 3, 1);
    CHECK(out == std::string(esc("2A")) + "\r");

    out.clear();
    ansi::append_move(out, 1, 1, 20, 30);  // 远距离：只能绝对定位
    CHECK(out == esc("20;30H"));
}

TEST_CASE("term: Terminal 构造即套 daemon_safe —— 非 tty 下不发任何状态序列") {
    Terminal terminal;  // 默认 alt_screen / hide_cursor 全开

    if (Terminal::stdout_is_tty()) {
        WARN("stdout 是 tty：跳过非 tty 断言（交互终端下无法复现管道场景）");
        return;
    }

    CHECK_FALSE(terminal.options().alt_screen);
    CHECK_FALSE(terminal.options().hide_cursor);
    CHECK_FALSE(terminal.options().raw_mode);
    CHECK(terminal.caps().color_depth == ColorDepth::None);

    CHECK(terminal.enter() == ErrorCode::Ok);  // 无可发序列 → 零字节
    CHECK(terminal.entered());
    CHECK_FALSE(terminal.options().alt_screen);
    CHECK(terminal.leave() == ErrorCode::Ok);
    CHECK_FALSE(terminal.entered());
}

TEST_CASE("term: 未 enter 就 leave 是安全的空操作") {
    Terminal terminal;
    CHECK(terminal.leave() == ErrorCode::Ok);
    CHECK_FALSE(terminal.entered());
}
