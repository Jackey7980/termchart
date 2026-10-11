// parser_base.hpp —— 递归下降解析框架（L17）
//
// 所有方言共用同一套"读 token 的手感"：peek/advance/match/skip_line + 诊断记录。
// 设计取向（与 core/error.hpp 的错误策略一致）：
//   * 解析**不抛异常**：错误记录进 ParseDiagnostic，由方言入口转成 Result 的错误码；
//   * 只保留**第一个**错误（后续错误往往是它引发的连锁噪声）；
//   * 关键不变量：任何循环都必须保证 index 前进，否则 fuzz 输入会让解析器挂死。
#ifndef TERMCHART_DSL_PARSER_BASE_HPP
#define TERMCHART_DSL_PARSER_BASE_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/core/error.hpp"
#include "termchart/export.hpp"
#include "termchart/dsl/lexer.hpp"

namespace termchart::dsl {

struct ParseDiagnostic {
    ErrorCode code = ErrorCode::Ok;
    std::string message;
    std::size_t line = 1;
    std::size_t column = 1;
    std::size_t offset = 0;

    [[nodiscard]] bool ok() const noexcept { return code == ErrorCode::Ok; }
};

class ParserBase {
public:
    TERMCHART_API ParserBase(std::vector<Token> tokens, std::string dialect,
                             std::string_view source = {});
    virtual ~ParserBase() = default;

    [[nodiscard]] TERMCHART_API const ParseDiagnostic& diagnostic() const noexcept;
    [[nodiscard]] TERMCHART_API bool failed() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t error_count() const noexcept;

protected:
    // ── 游标 ───────────────────────────────────────────────────────────
    [[nodiscard]] TERMCHART_API const Token& current() const noexcept;
    [[nodiscard]] TERMCHART_API const Token& peek(std::size_t ahead = 1) const noexcept;
    [[nodiscard]] TERMCHART_API bool at_end() const noexcept;
    TERMCHART_API Token advance() noexcept;
    TERMCHART_API void skip_newlines() noexcept;
    [[nodiscard]] TERMCHART_API std::size_t token_index() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t token_count() const noexcept;

    // ── 匹配（匹配成功才前进） ─────────────────────────────────────────
    [[nodiscard]] TERMCHART_API bool check_punct(std::string_view text) const noexcept;
    [[nodiscard]] TERMCHART_API bool check_keyword(std::string_view text) const noexcept;
    [[nodiscard]] TERMCHART_API bool check_kind(TokenKind kind) const noexcept;
    TERMCHART_API bool match_punct(std::string_view text) noexcept;
    TERMCHART_API bool match_keyword(std::string_view text) noexcept;
    TERMCHART_API bool match_identifier(std::string_view text) noexcept;
    TERMCHART_API bool match_kind(TokenKind kind) noexcept;

    [[nodiscard]] TERMCHART_API std::string_view previous_text() const noexcept;
    [[nodiscard]] TERMCHART_API const Token& previous_token() const noexcept;

    // ── 诊断 ───────────────────────────────────────────────────────────
    TERMCHART_API void report(ErrorCode code, std::string message) noexcept;
    TERMCHART_API void report_at(const Token& token, ErrorCode code,
                                 std::string message) noexcept;
    // 声称"到此结束"：若当前不是行尾/流尾，记为 ParseError（避免静默吞掉尾部）。
    TERMCHART_API bool expect_end_of_statement() noexcept;
    // 跳过本行剩余 token（用于"认得出但本切片不做"的指令）。
    TERMCHART_API void skip_to_end_of_line() noexcept;
    // 跳过一个配对的括号区（含嵌套），用于忽略整段指令。
    TERMCHART_API void skip_balanced(std::string_view open, std::string_view close) noexcept;

    [[nodiscard]] TERMCHART_API std::string_view source() const noexcept;
    [[nodiscard]] TERMCHART_API const std::string& dialect() const noexcept;

private:
    std::vector<Token> tokens_;
    std::size_t index_ = 0;
    std::size_t last_index_ = 0;
    ParseDiagnostic diagnostic_;
    std::size_t error_count_ = 0;
    std::string dialect_;
    std::string_view source_;
};

}  // namespace termchart::dsl

#endif  // TERMCHART_DSL_PARSER_BASE_HPP
