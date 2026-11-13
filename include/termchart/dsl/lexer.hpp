// lexer.hpp —— DSL 词法分析器（L17）
//
// 各 DSL（mermaid / dot / plantuml / d2）的**词法骨架是同一个**：
// 标识符、字符串、数字、箭头、标点、换行、注释。差异只有两处，都以参数表达：
//   * 关键字集合（方言自定义）；
//   * 注释前缀（mermaid "%%"、dot "//" 与 "#"、plantuml "'"）。
//
// 契约：
//   * **永不抛异常**；非法输入产出 TokenKind::Invalid 并带位置，由 parser 决定是否致命；
//   * 位置（行列）从 1 开始，用于诊断信息，不参与语义；
//   * 长度为 0 的 token 只可能是 End（防止调用方死循环）。
#ifndef TERMCHART_DSL_LEXER_HPP
#define TERMCHART_DSL_LEXER_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/core/error.hpp"
#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"

namespace termchart::dsl {

enum class TokenKind : std::uint8_t {
    End = 0,
    Identifier,    // A、node_1、foo.bar
    Keyword,       // 方言关键字（大小写不敏感匹配）
    Number,        // 42、3.14
    Text,          // 引号字符串（引号已去掉、转义已还原）
    Arrow,         // -->、---、-.->、==>、<-->、--
    Punctuation,   // [ ] ( ) { } | ; , : = 等单字符
    Newline,       // 语句分隔
    Comment,       // 仅 keep_comments 时产出
    Invalid,       // 非法输入（未闭合字符串、怪异字符）
};

struct Token {
    TokenKind kind = TokenKind::End;
    std::string text;
    std::size_t offset = 0;   // 相对源串的字节偏移
    std::size_t length = 0;
    std::size_t line = 1;     // 1-based
    std::size_t column = 1;   // 1-based（按字节，足够定位）

    [[nodiscard]] bool is(TokenKind expected) const noexcept { return kind == expected; }
    [[nodiscard]] bool is_punct(std::string_view expected) const noexcept {
        return kind == TokenKind::Punctuation && text == expected;
    }
};

// 关键字集合：大小写不敏感。用 span 指向方言语义里的静态表，不持有所有权。
class KeywordSet {
public:
    constexpr KeywordSet() noexcept = default;
    constexpr explicit KeywordSet(std::span<const std::string_view> words) noexcept
        : words_(words) {}

    [[nodiscard]] TERMCHART_API bool contains(std::string_view word) const noexcept;
    [[nodiscard]] constexpr std::size_t size() const noexcept { return words_.size(); }

private:
    std::span<const std::string_view> words_{};
};

struct LexerOptions {
    bool keep_comments = false;   // 解析器通常不需要注释
    bool keep_newlines = true;    // 只有"整篇重排"的场景才关掉
    // 注释前缀；默认按 mermaid。dot 用 {"//", "#"}，plantuml 用 {"'"}。
    std::vector<std::string_view> comment_prefixes{"%%"};
    std::size_t max_tokens = 200000;  // 防御性上限（fuzz 输入可能极长）
};

class Lexer {
public:
    TERMCHART_API Lexer(std::string_view source, KeywordSet keywords,
                        LexerOptions options = {});
    // 每次调用前进一个 token；到末尾后恒返回 End。
    TERMCHART_API Token next() noexcept;
    // 一次性取完（含结尾的 End），超过 max_tokens 返回 OutOfRange。
    TERMCHART_API Result<std::vector<Token>> tokenize() noexcept;

    [[nodiscard]] std::size_t offset() const noexcept { return offset_; }
    [[nodiscard]] std::string_view source() const noexcept { return source_; }

private:
    std::string_view source_;
    KeywordSet keywords_{};
    LexerOptions options_{};
    std::size_t offset_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
};

// 便捷入口：一次调用拿到全部 token。
TERMCHART_API Result<std::vector<Token>> tokenize(std::string_view source, KeywordSet keywords,
                                                  LexerOptions options = {});

}  // namespace termchart::dsl

#endif  // TERMCHART_DSL_LEXER_HPP
