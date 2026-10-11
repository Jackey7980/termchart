// test_protocol_detect.cpp —— 环境探测
//
// 探测读的是**进程全局环境**，所以这组用例自己负责"设值 → 断言 → 复原"，并把
// 与被测项无关的来源一并中性化，保证在任意开发机/CI 上都确定。选择逻辑
// （pick_protocol）在 test_protocol_fallback.cpp 里单独测，两者互不耦合。
#include <doctest.h>

#include <cstdlib>
#include <string>

#include "termchart/protocol/detect.hpp"

namespace {

using namespace termchart;
using namespace termchart::protocol;

// 临时设置环境变量并在作用域结束时复原（含"变量原本不存在"的情形）。
// 传空串表示"让它看起来没设"：Windows 的 _putenv_s 会移除变量，POSIX 下为空值。
class ScopedEnv {
public:
    ScopedEnv(const char* name, const char* value) : name_(name) {
        if (const char* old = std::getenv(name)) {
            had_old_ = true;
            old_ = old;
        }
        set(value);
    }
    ~ScopedEnv() {
        if (had_old_) {
            set(old_.c_str());
        } else {
            set("");
        }
    }
    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;

private:
    void set(const char* value) {
#if defined(_WIN32)
        (void)_putenv_s(name_.c_str(), value);
#else
        (void)setenv(name_.c_str(), value, 1);
#endif
    }

    std::string name_;
    std::string old_;
    bool had_old_ = false;
};

// 把所有与 sixel 有关的来源压平，只留下被测的那一个。
struct SixelIsolation {
    ScopedEnv term{"TERM", "xterm-256color"};
    ScopedEnv force_off{"TERMCHART_SIXEL", ""};
};

}  // namespace

TEST_CASE("protocol/detect: Konsole 要看版本号，不能只看变量存在") {
    SixelIsolation isolation;
    {
        ScopedEnv version("KONSOLE_VERSION", "210800");  // 早于 sixel 支持
        CHECK_FALSE(detect_protocol_caps().sixel);
    }
    {
        ScopedEnv version("KONSOLE_VERSION", "220400");  // 首个支持 sixel 的版本
        CHECK(detect_protocol_caps().sixel);
    }
    {
        ScopedEnv version("KONSOLE_VERSION", "230801");
        CHECK(detect_protocol_caps().sixel);
    }
    {
        ScopedEnv version("KONSOLE_VERSION", "");  // 没设就该是 false
        CHECK_FALSE(detect_protocol_caps().sixel);
    }
}

TEST_CASE("protocol/detect: TERMCHART_* 手动覆盖开关生效") {
    {
        ScopedEnv iterm2("TERMCHART_ITERM2", "1");
        CHECK(detect_protocol_caps().iterm2);
    }
    {
        ScopedEnv sixel("TERMCHART_SIXEL", "1");
        CHECK(detect_protocol_caps().sixel);
    }
}

TEST_CASE("protocol/detect: 探测不到时一律 false，不猜") {
    ScopedEnv term("TERM", "xterm-256color");
    ScopedEnv term_program("TERM_PROGRAM", "Apple_Terminal");
    ScopedEnv lc_terminal("LC_TERMINAL", "");
    ScopedEnv kitty("KITTY_WINDOW_ID", "");
    ScopedEnv force_sixel("TERMCHART_SIXEL", "");
    ScopedEnv force_iterm2("TERMCHART_ITERM2", "");
    ScopedEnv konsole("KONSOLE_VERSION", "");

    const ProtocolCaps caps = detect_protocol_caps();
    CHECK_FALSE(caps.kitty);
    CHECK_FALSE(caps.iterm2);
    CHECK_FALSE(caps.sixel);
}

TEST_CASE("protocol/detect: iTerm2 与 WezTerm 都按 OSC 1337 能力识别") {
    ScopedEnv term("TERM", "xterm-256color");
    ScopedEnv force_iterm2("TERMCHART_ITERM2", "");
    {
        ScopedEnv program("TERM_PROGRAM", "iTerm.app");
        CHECK(detect_protocol_caps().iterm2);
    }
    {
        ScopedEnv program("TERM_PROGRAM", "WezTerm");
        CHECK(detect_protocol_caps().iterm2);
    }
    {
        ScopedEnv program("TERM_PROGRAM", "Apple_Terminal");
        ScopedEnv lc("LC_TERMINAL", "iTerm2");
        CHECK(detect_protocol_caps().iterm2);  // LC_TERMINAL 是备选来源
    }
}
