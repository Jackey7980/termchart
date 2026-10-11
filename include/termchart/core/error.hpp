// error.hpp —— 错误枚举与类别
//
// 错误处理边界见 docs/design/error_handling.md：
//   * **可恢复的、用户可能遇到的**失败 → Expected / Result<T>（本文件 + expected.hpp）；
//   * **契约破坏 / 内部不变量** → util/assert.hpp（Release 下 abort）；
//   * 渲染热路径一律不用异常：改为返回 Result 或钳位。
#ifndef TERMCHART_CORE_ERROR_HPP
#define TERMCHART_CORE_ERROR_HPP

#include <cstdint>
#include <string_view>

#include "termchart/export.hpp"

namespace termchart {

enum class ErrorCode : std::int32_t {
    Ok = 0,

    // 领域 / 参数
    InvalidArgument = 100,
    OutOfRange = 101,
    Unsupported = 102,
    NotImplemented = 103,
    NotInitialized = 104,

    // 解析 / 编码
    MalformedUtf8 = 200,
    ParseError = 201,

    // 平台 / IO
    IoError = 300,
    TerminalUnavailable = 301,
    PlatformError = 302,
};

// 错误类别：给日志与上层分流使用，不与具体码一一对应。
enum class ErrorCategory : std::uint8_t {
    None = 0,
    Logic,       // 调用方契约问题，改代码才能解决
    Domain,      // 数据/参数不合经营范围
    Encoding,    // UTF-8 / 编码相关
    Platform,    // IO / 终端 / OS
};

// constexpr 函数隐含 inline，因此定义必须留在头文件里；to_string/describe 在 .cpp。
constexpr std::string_view category_of(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Ok:
            return "none";
        case ErrorCode::InvalidArgument:
        case ErrorCode::OutOfRange:
        case ErrorCode::NotInitialized:
            return "logic";
        case ErrorCode::Unsupported:
        case ErrorCode::NotImplemented:
            return "domain";
        case ErrorCode::MalformedUtf8:
        case ErrorCode::ParseError:
            return "encoding";
        case ErrorCode::IoError:
        case ErrorCode::TerminalUnavailable:
        case ErrorCode::PlatformError:
            return "platform";
    }
    return "none";
}

// 稳定英文标识：不随语言/文案变化，可用于断言与测试。
const char* to_string(ErrorCode code) noexcept;

// 便于打日志的人类可读描述（英文，避免编码问题）。
const char* describe(ErrorCode code) noexcept;

constexpr bool is_ok(ErrorCode code) noexcept { return code == ErrorCode::Ok; }

}  // namespace termchart

#endif  // TERMCHART_CORE_ERROR_HPP
