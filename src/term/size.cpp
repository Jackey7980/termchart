#include "termchart/term/size.hpp"

#include <cstdlib>
#include <optional>
#include <string_view>

#include "src/term/platform_internal.hpp"

namespace termchart::term {
namespace {

std::string_view env_text(const char* name) noexcept {
#if defined(_WIN32)
    static thread_local char buffer[64]{};
    std::size_t required = 0;
    if (getenv_s(&required, buffer, sizeof(buffer), name) != 0 || required == 0) {
        return {};
    }
    return std::string_view{buffer};
#else
    const char* raw = std::getenv(name);
    return raw == nullptr ? std::string_view{} : std::string_view{raw};
#endif
}

std::optional<int> env_int(const char* name) noexcept {
    const std::string_view text = env_text(name);
    int value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
        value = value * 10 + (ch - '0');
        if (value > 65535) {
            return std::nullopt;
        }
    }
    return value;
}

}  // namespace

TerminalSize size_from_env() noexcept {
    TerminalSize size;
    if (const auto cols = env_int("COLUMNS")) {
        size.cols = static_cast<std::uint16_t>(*cols);
    }
    if (const auto rows = env_int("LINES")) {
        size.rows = static_cast<std::uint16_t>(*rows);
    }
    return size;
}

TerminalSize query_size() noexcept {
    const platform::SizeResult platform_size = platform::query_terminal_size();
    if (platform_size.ok) {
        TerminalSize size;
        size.cols = platform_size.cols;
        size.rows = platform_size.rows;
        return size;
    }
    const TerminalSize from_env = size_from_env();
    if (from_env.valid()) {
        return from_env;
    }
    return TerminalSize{};  // 80×24 兜底
}

}  // namespace termchart::term
