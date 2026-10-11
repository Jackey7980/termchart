#include "termchart/term/terminal.hpp"

#include <cstdio>
#include <string>

#include "src/term/platform_internal.hpp"
#include "termchart/detail/ansi_tables.hpp"
#include "termchart/term/ansi.hpp"
#include "termchart/term/daemon_safe.hpp"
#include "termchart/util/logging.hpp"

namespace termchart::term {

namespace {

std::string_view env_var(const char* name) noexcept {
    const char* raw = std::getenv(name);
    return raw == nullptr ? std::string_view{} : std::string_view{raw};
}

CapEnvironment environment_snapshot() noexcept {
    CapEnvironment env;
    env.term = env_var("TERM");
    env.colorterm = env_var("COLORTERM");
    env.term_program = env_var("TERM_PROGRAM");
    env.wt_session = env_var("WT_SESSION");
    env.ci = std::getenv("CI") != nullptr;
    return env;
}

Status write_to_stdout(std::string_view bytes) noexcept {
    if (bytes.empty()) {
        return ErrorCode::Ok;
    }
    const std::size_t written = std::fwrite(bytes.data(), 1, bytes.size(), stdout);
    if (written != bytes.size()) {
        TERMCHART_LOG_ERROR("failed to write terminal escape sequence");
        return ErrorCode::IoError;
    }
    return ErrorCode::Ok;
}

std::string mouse_sequence(bool enable) noexcept {
    std::string sequence;
    sequence += std::string(detail::kCsiPrefix);
    sequence += enable ? "?1000h" : "?1000l";
    sequence += std::string(detail::kCsiPrefix);
    sequence += enable ? "?1006h" : "?1006l";
    return sequence;
}

}  // namespace

Terminal::Terminal(TerminalOptions options) noexcept : options_(options) {
    const bool tty = stdout_is_tty();
    const CapEnvironment env = environment_snapshot();
    const DaemonSafeSettings safe = daemon_safe_for(tty, env.ci);
    options_ = apply(options_, safe);
    caps_ = apply(detect(env, tty), safe);
}

Terminal::~Terminal() { restore(); }

Terminal::Terminal(Terminal&& other) noexcept
    : options_(other.options_),
      raw_mode_(std::move(other.raw_mode_)),
      changes_(other.changes_),
      entered_(other.entered_),
      caps_(other.caps_) {
    other.changes_ = Changes{};
    other.entered_ = false;
}

Terminal& Terminal::operator=(Terminal&& other) noexcept {
    if (this != &other) {
        restore();
        options_ = other.options_;
        raw_mode_ = std::move(other.raw_mode_);
        changes_ = other.changes_;
        entered_ = other.entered_;
        caps_ = other.caps_;
        other.changes_ = Changes{};
        other.entered_ = false;
    }
    return *this;
}

Status Terminal::enter() noexcept {
    if (entered_) {
        return ErrorCode::Ok;
    }
    // Windows 控制台默认不解释 ANSI 转义序列，先开启 VT 处理（否则 SGR 等序列裸露）。
    (void)platform::enable_vt();
    std::string sequence;
    if (options_.alt_screen) {
        sequence += ansi::enter_alt_screen();
    }
    if (options_.hide_cursor) {
        sequence += ansi::hide_cursor();
    }
    if (options_.enable_mouse) {
        sequence += mouse_sequence(true);
    }
    if (!sequence.empty()) {
        const Status status = write_to_stdout(sequence);
        if (status != ErrorCode::Ok) {
            return status;
        }
        changes_.alt_screen = options_.alt_screen;
        changes_.cursor_hidden = options_.hide_cursor;
        changes_.mouse_enabled = options_.enable_mouse;
    }
    if (options_.raw_mode) {
        raw_mode_.enter();
    }
    entered_ = true;
    return ErrorCode::Ok;
}

Status Terminal::leave() noexcept {
    if (!entered_) {
        return ErrorCode::Ok;
    }
    std::string sequence;
    if (changes_.mouse_enabled) {
        sequence += mouse_sequence(false);
    }
    if (changes_.cursor_hidden) {
        sequence += ansi::show_cursor();
    }
    if (changes_.alt_screen) {
        sequence += ansi::exit_alt_screen();
    }
    Status status = ErrorCode::Ok;
    if (!sequence.empty()) {
        status = write_to_stdout(sequence);
    }
    raw_mode_.leave();
    (void)platform::disable_vt();  // 还原 VT 模式（原本就开着 VT 的终端保持原状）
    std::fflush(stdout);
    changes_ = Changes{};
    entered_ = false;
    return status;
}

void Terminal::restore() noexcept {
    if (!entered_) {
        return;
    }
    (void)leave();
}

Status Terminal::write(std::string_view bytes) noexcept { return write_to_stdout(bytes); }

Status Terminal::flush() noexcept {
    return std::fflush(stdout) == 0 ? ErrorCode::Ok : ErrorCode::IoError;
}

TerminalSize Terminal::size() const noexcept { return query_size(); }

TerminalCaps Terminal::caps() const noexcept { return caps_; }

bool Terminal::stdout_is_tty() noexcept { return platform::stdout_is_tty(); }

bool Terminal::stdin_is_tty() noexcept { return platform::stdin_is_tty(); }

}  // namespace termchart::term
