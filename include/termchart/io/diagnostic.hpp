// io/diagnostic.hpp —— 解析诊断（header-only，L2）
//
// 从 dsl/parser_base.hpp 下沉而来：io 的 csv/json 解析需要同口径的诊断
// （ErrorCode + 行列 + 字节偏移），而 io=2 不能 include dsl=17（层号硬墙）。
// dsl/parser_base.hpp 现以 using 别名 re-export 本类型，既有调用点零改动。
// 设计口径（与 core/error.hpp 的错误策略一致）：
//   * 解析不抛异常：错误记录进 ParseDiagnostic，由入口转成 Result 错误码；
//   * 只保留第一个错误（后续错误往往是它引发的连锁噪声）。
#ifndef TERMCHART_IO_DIAGNOSTIC_HPP
#define TERMCHART_IO_DIAGNOSTIC_HPP

#include <cstddef>
#include <string>

#include "termchart/core/error.hpp"

namespace termchart::io {

struct ParseDiagnostic {
    ErrorCode code = ErrorCode::Ok;
    std::string message;
    std::size_t line = 1;
    std::size_t column = 1;
    std::size_t offset = 0;

    [[nodiscard]] bool ok() const noexcept { return code == ErrorCode::Ok; }
};

}  // namespace termchart::io

#endif  // TERMCHART_IO_DIAGNOSTIC_HPP
