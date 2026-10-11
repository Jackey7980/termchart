// logging.hpp —— 分级日志，编译期可关
//
// 两级开关：
//   * 运行期 set_log_level()：默认 Info；
//   * 编译期 TERMCHART_MAX_LOG_LEVEL（0..5），高于它的 TERMCHART_LOG_* 直接编译消失，
//     保证 Release 里日志分支零成本。
// sink 可替换：测试用 capture_log() 抓取输出，不写 stderr。
#ifndef TERMCHART_UTIL_LOGGING_HPP
#define TERMCHART_UTIL_LOGGING_HPP

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/export.hpp"

#ifndef TERMCHART_MAX_LOG_LEVEL
#define TERMCHART_MAX_LOG_LEVEL 3  // Info
#endif

namespace termchart::util {

enum class LogLevel : std::uint8_t {
    Off = 0,
    Error = 1,
    Warn = 2,
    Info = 3,
    Debug = 4,
    Trace = 5,
};

TERMCHART_API void set_log_level(LogLevel level) noexcept;
TERMCHART_API LogLevel log_level() noexcept;
TERMCHART_API const char* level_name(LogLevel level) noexcept;

TERMCHART_API void write_log(LogLevel level, std::string_view message) noexcept;

// 替换输出目标（测试/嵌入式）。设为 nullptr 恢复默认（stderr）。
TERMCHART_API void set_log_sink(std::function<void(LogLevel, std::string_view)> sink);

// 测试辅助：接管 sink 并记录全部条目；返回即式 RAII 由调用方手动 flush_log_capture()。
TERMCHART_API void capture_log_begin() noexcept;
TERMCHART_API std::vector<std::string> capture_log_end() noexcept;

}  // namespace termchart::util

#define TERMCHART_LOG_AT(level_enum, msg)                                              \
    do {                                                                               \
        using ::termchart::util::LogLevel;                                             \
        constexpr int kLevelValue = static_cast<int>(level_enum);                      \
        if constexpr (kLevelValue <= TERMCHART_MAX_LOG_LEVEL) {                        \
            if (kLevelValue <= static_cast<int>(::termchart::util::log_level())) {     \
                ::termchart::util::write_log(level_enum, (msg));                       \
            }                                                                          \
        }                                                                              \
    } while (false)

#define TERMCHART_LOG_ERROR(msg) TERMCHART_LOG_AT(::termchart::util::LogLevel::Error, msg)
#define TERMCHART_LOG_WARN(msg) TERMCHART_LOG_AT(::termchart::util::LogLevel::Warn, msg)
#define TERMCHART_LOG_INFO(msg) TERMCHART_LOG_AT(::termchart::util::LogLevel::Info, msg)
#define TERMCHART_LOG_DEBUG(msg) TERMCHART_LOG_AT(::termchart::util::LogLevel::Debug, msg)
#define TERMCHART_LOG_TRACE(msg) TERMCHART_LOG_AT(::termchart::util::LogLevel::Trace, msg)

#endif  // TERMCHART_UTIL_LOGGING_HPP
