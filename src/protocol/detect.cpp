// detect.cpp —— 环境变量探测
#include "termchart/protocol/detect.hpp"

#include <cstdlib>
#include <cstring>

namespace termchart::protocol {

namespace {

bool env_contains(const char* name, const char* needle) noexcept {
    const char* value = std::getenv(name);
    return value != nullptr && std::strstr(value, needle) != nullptr;
}

bool env_set(const char* name) noexcept {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0';
}

// 环境变量里的十进制数字是否 >= minimum。用于"某终端从某版本才开始支持某协议"
// 这类判定——只看变量是否存在会把老版本喂成乱码。
bool env_number_at_least(const char* name, long minimum) noexcept {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return false;
    }
    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    return end != value && parsed >= minimum;
}

}  // namespace

// 探测依据（只看环境变量，不写任何终端查询序列——那需要读写终端，属于 term/ 的职责）：
//   Kitty    ：KITTY_WINDOW_ID / TERM*kitty / TERM_PROGRAM*kitty
//   Iterm2   ：TERM_PROGRAM*iTerm / LC_TERMINAL*iTerm（iTerm2 两者都设）
//              / TERM_PROGRAM*WezTerm（WezTerm 实现了 OSC 1337 File）
//   Sixel    ：TERMCHART_SIXEL 手动覆盖 / TERM*sixel / TERM*mlterm / KONSOLE_VERSION
//   TrueColor：COLORTERM 含 truecolor 或 24bit
// 探测不到一律 false：宁可降级到半块，也不要发出终端不认的字节流。
// `TERMCHART_ITERM2` 与 `TERMCHART_SIXEL` 一样，是给用户/测试用的手动覆盖开关。
ProtocolCaps detect_protocol_caps() noexcept {
    ProtocolCaps caps;
    caps.kitty = env_set("KITTY_WINDOW_ID") || env_contains("TERM", "kitty") ||
                 env_contains("TERM_PROGRAM", "kitty");
    caps.iterm2 = env_set("TERMCHART_ITERM2") || env_contains("TERM_PROGRAM", "iTerm") ||
                  env_contains("LC_TERMINAL", "iTerm") ||
                  env_contains("TERM_PROGRAM", "WezTerm");
    // Konsole 从 22.04（KONSOLE_VERSION >= 220400）起支持 sixel，老版本会显示乱码，
    // 因此这里必须比版本号，不能只看变量是否存在。
    caps.sixel = env_set("TERMCHART_SIXEL") || env_contains("TERM", "sixel") ||
                 env_contains("TERM", "mlterm") ||
                 env_number_at_least("KONSOLE_VERSION", 220400);
    caps.truecolor = env_contains("COLORTERM", "truecolor") ||
                     env_contains("COLORTERM", "24bit");
    return caps;
}

}  // namespace termchart::protocol
