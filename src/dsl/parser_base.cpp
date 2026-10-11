#include "termchart/dsl/parser_base.hpp"

namespace termchart::dsl {
namespace {

// 流尾哨兵：tokens_ 一定以 End 结尾，但越界索引也要有东西可返回。
const Token& end_token() noexcept {
    static const Token sentinel{};
    return sentinel;
}

}  // namespace

ParserBase::ParserBase(std::vector<Token> tokens, std::string dialect, std::string_view source)
    : tokens_(std::move(tokens)), dialect_(std::move(dialect)), source_(source) {
    if (tokens_.empty() || tokens_.back().kind != TokenKind::End) {
        Token end;
        end.kind = TokenKind::End;
        tokens_.push_back(std::move(end));
    }
}

const ParseDiagnostic& ParserBase::diagnostic() const noexcept { return diagnostic_; }

bool ParserBase::failed() const noexcept { return diagnostic_.code != ErrorCode::Ok; }

std::size_t ParserBase::error_count() const noexcept { return error_count_; }

const Token& ParserBase::current() const noexcept {
    return index_ < tokens_.size() ? tokens_[index_] : end_token();
}

const Token& ParserBase::peek(std::size_t ahead) const noexcept {
    const std::size_t target = index_ + ahead;
    return target < tokens_.size() ? tokens_[target] : end_token();
}

bool ParserBase::at_end() const noexcept { return current().kind == TokenKind::End; }

Token ParserBase::advance() noexcept {
    last_index_ = index_;
    const Token token = current();
    if (index_ < tokens_.size() && token.kind != TokenKind::End) {
        ++index_;  // End 上不再前进：调用方循环条件靠 at_end()
    }
    return token;
}

void ParserBase::skip_newlines() noexcept {
    while (current().kind == TokenKind::Newline || current().kind == TokenKind::Comment) {
        ++index_;
    }
}

std::size_t ParserBase::token_index() const noexcept { return index_; }

std::size_t ParserBase::token_count() const noexcept { return tokens_.size(); }

bool ParserBase::check_punct(std::string_view text) const noexcept {
    return current().is_punct(text);
}

bool ParserBase::check_keyword(std::string_view text) const noexcept {
    if (current().kind != TokenKind::Keyword || current().text.size() != text.size()) {
        return false;
    }
    // 关键字大小写不敏感（mermaid 写 `classDef`，关键字表里是 `classdef`）。
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char lhs = current().text[i];
        const char rhs = text[i];
        const char lhs_lower = (lhs >= 'A' && lhs <= 'Z') ? static_cast<char>(lhs - 'A' + 'a') : lhs;
        const char rhs_lower = (rhs >= 'A' && rhs <= 'Z') ? static_cast<char>(rhs - 'A' + 'a') : rhs;
        if (lhs_lower != rhs_lower) {
            return false;
        }
    }
    return true;
}

bool ParserBase::check_kind(TokenKind kind) const noexcept { return current().kind == kind; }

bool ParserBase::match_punct(std::string_view text) noexcept {
    if (!check_punct(text)) {
        return false;
    }
    (void)advance();
    return true;
}

bool ParserBase::match_keyword(std::string_view text) noexcept {
    if (!check_keyword(text)) {
        return false;
    }
    (void)advance();
    return true;
}

bool ParserBase::match_identifier(std::string_view text) noexcept {
    if (current().kind != TokenKind::Identifier || current().text != text) {
        return false;
    }
    (void)advance();
    return true;
}

bool ParserBase::match_kind(TokenKind kind) noexcept {
    if (current().kind != kind) {
        return false;
    }
    (void)advance();
    return true;
}

std::string_view ParserBase::previous_text() const noexcept { return previous_token().text; }

const Token& ParserBase::previous_token() const noexcept {
    return last_index_ < tokens_.size() ? tokens_[last_index_] : end_token();
}

void ParserBase::report(ErrorCode code, std::string message) noexcept {
    report_at(current(), code, std::move(message));
}

void ParserBase::report_at(const Token& token, ErrorCode code, std::string message) noexcept {
    ++error_count_;
    if (!diagnostic_.ok()) {
        return;  // 只保留第一个错误
    }
    diagnostic_.code = code;
    diagnostic_.message = std::move(message);
    diagnostic_.line = token.line;
    diagnostic_.column = token.column;
    diagnostic_.offset = token.offset;
}

bool ParserBase::expect_end_of_statement() noexcept {
    if (at_end() || current().kind == TokenKind::Newline || check_punct(";")) {
        return true;
    }
    report(ErrorCode::ParseError, "语句尾部有多余内容");
    skip_to_end_of_line();
    return false;
}

void ParserBase::skip_to_end_of_line() noexcept {
    while (!at_end() && current().kind != TokenKind::Newline) {
        ++index_;
    }
}

void ParserBase::skip_balanced(std::string_view open, std::string_view close) noexcept {
    if (!match_punct(open)) {
        return;
    }
    int depth = 1;
    while (!at_end() && depth > 0) {
        const Token token = advance();
        if (token.is_punct(open)) {
            ++depth;
        } else if (token.is_punct(close)) {
            --depth;
        }
    }
}

std::string_view ParserBase::source() const noexcept { return source_; }

const std::string& ParserBase::dialect() const noexcept { return dialect_; }

}  // namespace termchart::dsl
