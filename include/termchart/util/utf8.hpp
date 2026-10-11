// utf8.hpp —— UTF-8 编解码，对非法序列容错
//
// 容错策略（这是本文件的核心决策）：**永不抛异常、永不 UB**。
// 输入可能来自 CSV、来自管道、来自 fuzz（tests/fuzz/fuzz_utf8.cpp）：
//   * 非法序列 → 产出 U+FFFD，**只消耗 1 字节**后继续（保证游标一定前进）；
//   * 截断序列 → 同上；调用方可选地把 "consumed < 期望" 当作"数据还没到"。
#ifndef TERMCHART_UTIL_UTF8_HPP
#define TERMCHART_UTIL_UTF8_HPP

#include <cstddef>
#include <string>
#include <string_view>

#include "termchart/core/error.hpp"
#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"

namespace termchart::util {

inline constexpr char32_t kReplacementChar = 0xFFFD;
inline constexpr std::size_t kMaxUtf8Bytes = 4;

struct Utf8Decode {
    char32_t cp = kReplacementChar;   // 解码出的码点（失败时为 U+FFFD）
    int consumed = 0;                 // 消耗的字节数；0 表示 pos 已越界
    bool ok = false;                  // 是否为合法且完整的序列
    bool incomplete = false;          // 合法前缀但字节不够（流式场景用）
};

// 从 pos 处解码一个码点。pos 越界返回 consumed == 0。
TERMCHART_API Utf8Decode decode(std::string_view bytes, std::size_t pos) noexcept;

// 编码码点到 UTF-8，返回写入字节数（0 表示不可编码：代理码点或 > U+10FFFF）。
TERMCHART_API std::size_t encode(char32_t cp, char* out) noexcept;

TERMCHART_API void append(std::string& out, char32_t cp);
TERMCHART_API std::string to_utf8(char32_t cp);

// 码点数（非法序列按一个 U+FFFD 计）。
TERMCHART_API std::size_t codepoint_count(std::string_view text) noexcept;

// 严格校验：整个串是否为合法 UTF-8（解析 CSV 头、DSL 输入时先过一遍）。
TERMCHART_API bool is_valid(std::string_view text) noexcept;

// 显式报错版本：给"必须干净"的输入用。与上面的容错版并存，不互相替代。
TERMCHART_API Result<std::string> sanitize(std::string_view text) noexcept;

}  // namespace termchart::util

#endif  // TERMCHART_UTIL_UTF8_HPP
