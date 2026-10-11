// test_interaction_input.cpp —— 输入缓冲、字节流解析、分发
#include <doctest.h>

#include <string>
#include <vector>

#include "termchart/interaction/dispatcher.hpp"
#include "termchart/interaction/event.hpp"
#include "termchart/interaction/input.hpp"
#include "termchart/interaction/input_parser.hpp"
#include "termchart/interaction/key.hpp"
#include "termchart/interaction/mouse.hpp"

namespace {

using namespace termchart;
using namespace termchart::interaction;

}  // namespace

TEST_CASE("interaction/input: 缓冲与 pending") {
    Input in;
    CHECK(in.empty());
    in.push("ab");
    CHECK(in.size() == 2);
    CHECK(in.pending() == "ab");
    in.push('c');
    CHECK(in.size() == 3);
    in.clear();
    CHECK(in.empty());
}

TEST_CASE("interaction/parser: 方向键 / Home / End / Delete") {
    InputParser p;
    auto ev = p.feed("\x1b[A\x1b[B\x1b[C\x1b[D");
    REQUIRE(ev.size() == 4);
    CHECK(std::get<KeyEvent>(ev[0]).key == Key::Up);
    CHECK(std::get<KeyEvent>(ev[1]).key == Key::Down);
    CHECK(std::get<KeyEvent>(ev[2]).key == Key::Right);
    CHECK(std::get<KeyEvent>(ev[3]).key == Key::Left);

    ev = p.feed("\x1b[H\x1b[F\x1b[5~\x1b[3~");
    REQUIRE(ev.size() == 4);
    CHECK(std::get<KeyEvent>(ev[0]).key == Key::Home);
    CHECK(std::get<KeyEvent>(ev[1]).key == Key::End);
    CHECK(std::get<KeyEvent>(ev[2]).key == Key::PageUp);
    CHECK(std::get<KeyEvent>(ev[3]).key == Key::Delete);
}

TEST_CASE("interaction/parser: 未完成序列的缓冲有上限（不会被喂爆内存）") {
    InputParser p;
    // 一条永远等不到终止符的 OSC。真实终端也可能被喂进这种流：库不能因为对端
    // 不发 BEL/ST 就把内存吃光（fuzz 里用 `ESC ]` + 无穷多个字节就能触发）。
    const std::string flood =
        std::string("\x1b]") + std::string(InputParser::kMaxPendingBytes * 2, 'x');
    (void)p.feed(flood);
    CHECK(p.pending() <= InputParser::kMaxPendingBytes);

    // 超限之后必须能**重新同步**继续工作，而不是永久卡住。
    p.reset();
    const auto after = p.feed("\x1b[A");
    REQUIRE(after.size() == 1);
    CHECK(std::get<KeyEvent>(after[0]).key == Key::Up);
    CHECK(p.pending() == 0);

    // 溢出后若这批字节里没有 ESC，说明还处在被丢弃序列的中途：整批丢弃，
    // 且**不产出任何伪事件**（"保留尾部若干字节"会把 base64 中段当按键吐出来，
    // 那比丢数据更糟——这是审计指出后改掉的行为）。
    p.reset();
    (void)p.feed(flood);
    const auto junk = p.feed(std::string(1024, 'q'));
    CHECK(junk.empty());
    CHECK(p.pending() == 0);

    // 下一个 ESC 到达即恢复解析。
    const auto resumed = p.feed(std::string("zz") + "\x1b[A");
    REQUIRE(resumed.size() == 3);
    CHECK(std::get<KeyEvent>(resumed[2]).key == Key::Up);
    CHECK(p.pending() == 0);
}

TEST_CASE("interaction/parser: 超长 CSI 参数饱和而不溢出") {
    InputParser p;
    // 20 位数字远超 int：早期实现按 `value * 10 + digit` 累加会**有符号溢出（UB）**。
    const auto sgr = p.feed("\x1b[99999999999999999999m");
    CHECK(sgr.empty());       // SGR 不产出事件
    CHECK(p.pending() == 0);  // 但序列被完整消费，不留在缓冲里

    const auto unknown = p.feed("\x1b[99999999999999999999~");
    CHECK(unknown.empty());
    CHECK(p.pending() == 0);

    // 坐标被喂天文数字、按钮码正常：应正常产出"左键按下"（坐标饱和但不崩）。
    const auto mouse = p.feed("\x1b[<0;999999999999;999999999999M");
    REQUIRE(mouse.size() == 1);
    const MouseEvent& click = std::get<MouseEvent>(mouse[0]);
    CHECK(click.action == MouseAction::Press);
    CHECK(click.button == MouseButton::Left);

    // 连按钮码本身都是天文数字：饱和后 bit 64 被置上，按 SGR 语义归为"滚轮"——
    // 这是"参数大得离谱"的合理结果；本用例只要求**恰好一个事件且不崩**。
    const auto weird = p.feed("\x1b[<999999999999;1;1M");
    CHECK(weird.size() == 1);
}

TEST_CASE("interaction/parser: SS3 与功能键") {
    InputParser p;
    const auto ev = p.feed("\x1bOP\x1bOQ\x1b[15~\x1b[24~");
    REQUIRE(ev.size() == 4);
    CHECK(std::get<KeyEvent>(ev[0]).key == Key::F1);
    CHECK(std::get<KeyEvent>(ev[1]).key == Key::F2);
    CHECK(std::get<KeyEvent>(ev[2]).key == Key::F5);
    CHECK(std::get<KeyEvent>(ev[3]).key == Key::F12);
}

TEST_CASE("interaction/parser: 控制字符与可打印字符") {
    InputParser p;
    const auto ev = p.feed(std::string("a") + "\x0d\x09\x7f" + "\x01");
    REQUIRE(ev.size() == 5);
    CHECK(std::get<KeyEvent>(ev[0]).key == Key::Char);
    CHECK(std::get<KeyEvent>(ev[0]).ch == U'a');
    CHECK(std::get<KeyEvent>(ev[1]).key == Key::Enter);
    CHECK(std::get<KeyEvent>(ev[2]).key == Key::Tab);
    CHECK(std::get<KeyEvent>(ev[3]).key == Key::Backspace);
    CHECK(std::get<KeyEvent>(ev[4]).ch == U'a');
    CHECK(has_mod(std::get<KeyEvent>(ev[4]).mods, mod::kCtrl));
}

TEST_CASE("interaction/parser: UTF-8 与 Alt+字符") {
    InputParser p;
    auto ev = p.feed("\xe4\xb8\xad");  // 中
    REQUIRE(ev.size() == 1);
    CHECK(std::get<KeyEvent>(ev[0]).ch == U'\u4E2D');

    ev = p.feed("\x1bx");
    REQUIRE(ev.size() == 1);
    CHECK(std::get<KeyEvent>(ev[0]).ch == U'x');
    CHECK(has_mod(std::get<KeyEvent>(ev[0]).mods, mod::kAlt));
}

TEST_CASE("interaction/parser: 不完整序列留待后续字节") {
    InputParser p;
    CHECK(p.feed("\x1b[").empty());
    CHECK(p.pending() == 2);
    auto ev = p.feed("A");
    REQUIRE(ev.size() == 1);
    CHECK(std::get<KeyEvent>(ev[0]).key == Key::Up);
    CHECK(p.pending() == 0);

    CHECK(p.feed("\xe4\xb8").empty());  // UTF-8 跨包
    ev = p.feed("\xad");
    REQUIRE(ev.size() == 1);
    CHECK(std::get<KeyEvent>(ev[0]).ch == U'\u4E2D');
}

TEST_CASE("interaction/parser: 孤立 ESC 用 flush 收尾成 Escape") {
    InputParser p;
    CHECK(p.feed("\x1b").empty());
    const auto ev = p.flush();
    REQUIRE(ev.size() == 1);
    CHECK(std::get<KeyEvent>(ev[0]).key == Key::Escape);
    CHECK(p.pending() == 0);
}

TEST_CASE("interaction/parser: SGR 鼠标 按下/释放/滚轮/移动") {
    InputParser p;
    auto ev = p.feed("\x1b[<0;10;5M");  // 左键按下
    REQUIRE(ev.size() == 1);
    CHECK(std::get<MouseEvent>(ev[0]).action == MouseAction::Press);
    CHECK(std::get<MouseEvent>(ev[0]).button == MouseButton::Left);
    CHECK(std::get<MouseEvent>(ev[0]).x == 10);
    CHECK(std::get<MouseEvent>(ev[0]).y == 5);

    ev = p.feed("\x1b[<0;10;5m");  // 释放
    REQUIRE(ev.size() == 1);
    CHECK(std::get<MouseEvent>(ev[0]).action == MouseAction::Release);

    ev = p.feed("\x1b[<64;3;4M");  // 滚轮上
    REQUIRE(ev.size() == 1);
    CHECK(std::get<MouseEvent>(ev[0]).action == MouseAction::ScrollUp);

    ev = p.feed("\x1b[<32;7;8M");  // 移动
    REQUIRE(ev.size() == 1);
    CHECK(std::get<MouseEvent>(ev[0]).action == MouseAction::Move);
    CHECK(std::get<MouseEvent>(ev[0]).x == 7);
}

TEST_CASE("interaction/parser: OSC 跳过、未知 CSI 消费不出事件") {
    InputParser p;
    CHECK(p.feed("\x1b]0;title\x07").empty());
    CHECK(p.pending() == 0);
    CHECK(p.feed("\x1b[99X").empty());
    CHECK(p.pending() == 0);
}

TEST_CASE("interaction/dispatcher: 订阅顺序即优先级，消费即停") {
    Dispatcher d;
    std::vector<std::string> log;
    d.subscribe(EventKind::Key, [&](const Event& e) {
        log.push_back(std::string("first:") + kind_name(kind_of(e)));
        return false;
    });
    d.subscribe(EventKind::Key, [&](const Event&) {
        log.push_back("second");
        return true;
    });
    d.subscribe(EventKind::Key, [&](const Event&) {
        log.push_back("third");  // 不该被调用
        return false;
    });

    CHECK(d.handler_count() == 3);
    CHECK(d.dispatch(KeyEvent{Key::Enter}));
    REQUIRE(log.size() == 2);
    CHECK(log[0] == "first:key");
    CHECK(log[1] == "second");

    log.clear();
    CHECK_FALSE(d.dispatch(ResizeEvent{80, 24}));
    CHECK(log.empty());
}
