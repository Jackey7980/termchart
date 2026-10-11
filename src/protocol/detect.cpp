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

}  // namespace

ProtocolCaps detect_protocol_caps() noexcept {
    ProtocolCaps caps;
    caps.kitty = env_set("KITTY_WINDOW_ID") || env_contains("TERM", "kitty") ||
                 env_contains("TERM_PROGRAM", "kitty");
    caps.iterm2 = env_contains("TERM_PROGRAM", "iTerm");
    caps.sixel = env_set("TERMCHART_SIXEL") || env_contains("TERM", "sixel") ||
                 env_contains("TERM", "mlterm");
    caps.truecolor = env_contains("COLORTERM", "truecolor") ||
                     env_contains("COLORTERM", "24bit");
    return caps;
}

}  // namespace termchart::protocol
