// test_term_size_waker.cpp —— 终端尺寸查询（term/size）与跨线程唤醒原语（term/waker）
//
// 两者在 tests/ 里此前零命中：尺寸是"终端说了算"的外部状态，waker 是后台线程
// 唤醒主循环的唯一通道（app/post + select/poll 并发模型），都值得钉住行为。
#include <doctest.h>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>

#include "termchart/term/size.hpp"
#include "termchart/term/waker.hpp"

namespace {

using namespace termchart;
using namespace termchart::term;

void put_env(const char* name, const char* value) {
#if defined(_WIN32)
    (void)_putenv_s(name, value);  // value 为空串即删除该变量
#else
    if (value[0] == '\0') {
        (void)unsetenv(name);
    } else {
        (void)setenv(name, value, 1);
    }
#endif
}

// 保存并恢复环境变量，避免污染同进程里的其他用例。
class EnvGuard {
public:
    explicit EnvGuard(const char* name) : name_(name) {
        if (const char* old = std::getenv(name)) {
            old_ = old;
            had_ = true;
        }
    }
    ~EnvGuard() { put_env(name_.c_str(), had_ ? old_.c_str() : ""); }
    void set(const char* value) { put_env(name_.c_str(), value); }

private:
    std::string name_;
    std::string old_;
    bool had_ = false;
};

}  // namespace

TEST_CASE("term/size: TerminalSize 默认 80×24 且值语义完整") {
    const TerminalSize size;
    CHECK(size.cols == 80);
    CHECK(size.rows == 24);
    CHECK(size.valid());
    CHECK(size.as_point().x == doctest::Approx(80.0));
    CHECK(size.as_point().y == doctest::Approx(24.0));

    CHECK(TerminalSize{80, 24} == TerminalSize{80, 24});
    CHECK(TerminalSize{80, 24} != TerminalSize{81, 24});
    // 0 列/0 行不算有效尺寸（避免除零与零宽画布）。
    CHECK_FALSE(TerminalSize{0, 24}.valid());
    CHECK_FALSE(TerminalSize{80, 0}.valid());
}

TEST_CASE("term/size: size_from_env 读 COLUMNS/LINES，非法值回落默认") {
    EnvGuard cols("COLUMNS");
    EnvGuard lines("LINES");

    cols.set("132");
    lines.set("43");
    const TerminalSize parsed = size_from_env();
    CHECK(parsed.cols == 132);
    CHECK(parsed.rows == 43);
    CHECK(parsed.valid());

    // 非数字与超上界（>65535）都判为"没设"，保持 80×24 默认值。
    cols.set("abc");
    CHECK(size_from_env().cols == 80);
    cols.set("99999");
    CHECK(size_from_env().cols == 80);

    // "0" 是合法整数但零宽：解析得 0，valid() 为假（由调用方决定兜底）。
    cols.set("0");
    CHECK(size_from_env().cols == 0);
    CHECK_FALSE(size_from_env().valid());
}

TEST_CASE("term/size: query_size 在非 tty 下也必须给出可用尺寸") {
    // CI / 重定向到文件都没有 tty，平台查询失败后依次回落 env 与 80×24 兜底，
    // 契约是"永不返回 0"。
    const TerminalSize size = query_size();
    CHECK(size.valid());
    CHECK(size.cols >= 1);
    CHECK(size.rows >= 1);
}

TEST_CASE("term/waker: notify 唤醒、drain 清空、无信号时超时") {
    Waker waker;
    REQUIRE(waker.valid());
    REQUIRE(waker.native_handle() != 0);

    waker.notify();
    CHECK(waker.wait(500) == true);  // 被唤醒

    waker.drain();  // 消费掉已排队的唤醒
    CHECK(waker.wait(50) == false);  // 之后等到超时
}

TEST_CASE("term/waker: 移动后所有权随之转移") {
    Waker source;
    REQUIRE(source.valid());
    const std::uintptr_t handle = source.native_handle();

    Waker moved(std::move(source));
    CHECK(moved.valid());
    CHECK_FALSE(source.valid());
    CHECK(moved.native_handle() == handle);

    moved.notify();
    CHECK(moved.wait(500) == true);
}
