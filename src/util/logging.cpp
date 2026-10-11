#include "termchart/util/logging.hpp"

#include <cstdio>
#include <optional>

namespace termchart::util {
namespace {

struct LogState {
    LogLevel level = LogLevel::Info;
    std::function<void(LogLevel, std::string_view)> sink;
    std::optional<std::vector<std::string>> capture;
};

LogState& state() {
    static LogState s;  // function-local static：规避静态初始化顺序问题
    return s;
}

void default_sink(LogLevel level, std::string_view message) noexcept {
    std::fprintf(stderr, "[termchart:%s] %.*s\n", level_name(level),
                 static_cast<int>(message.size()), message.data());
}

}  // namespace

const char* level_name(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Off:
            return "off";
        case LogLevel::Error:
            return "error";
        case LogLevel::Warn:
            return "warn";
        case LogLevel::Info:
            return "info";
        case LogLevel::Debug:
            return "debug";
        case LogLevel::Trace:
            return "trace";
    }
    return "unknown";
}

void set_log_level(LogLevel level) noexcept { state().level = level; }

LogLevel log_level() noexcept { return state().level; }

void write_log(LogLevel level, std::string_view message) noexcept {
    if (level == LogLevel::Off) {
        return;
    }
    LogState& s = state();
    if (s.capture) {
        std::string line = "[";
        line += level_name(level);
        line += "] ";
        line.append(message);
        s.capture->push_back(std::move(line));
        return;
    }
    if (s.sink) {
        s.sink(level, message);
        return;
    }
    default_sink(level, message);
}

void set_log_sink(std::function<void(LogLevel, std::string_view)> sink) {
    state().sink = std::move(sink);
}

void capture_log_begin() noexcept { state().capture.emplace(); }

std::vector<std::string> capture_log_end() noexcept {
    LogState& s = state();
    std::vector<std::string> captured;
    if (s.capture) {
        captured = std::move(*s.capture);
        s.capture.reset();
    }
    return captured;
}

}  // namespace termchart::util
