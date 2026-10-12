#include "termchart/dsl/lexer.hpp"

#include <array>

#include "termchart/util/utf8.hpp"

namespace termchart::dsl {
namespace {

// 标识符的 ASCII 部分。非 ASCII 走"合法 UTF-8 序列才允许"的规则（见 next()）：
//   * 中文标签/边名必须能当标识符用（`A -- 失败 --> B`），否则解析器到处要打补丁；
//   * 但 0x80/0xFF 这类**非法 UTF-8** 仍然算 Invalid —— fuzz 输入要能被识别为坏数据，
//     而不是悄悄混进标识符。
bool is_identifier_start(char ch) noexcept {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

bool is_identifier_body(char ch) noexcept {
    return is_identifier_start(ch) || (ch >= '0' && ch <= '9') || ch == '.' || ch == '-';
}

bool is_digit(char ch) noexcept { return ch >= '0' && ch <= '9'; }

bool is_punctuation(char ch) noexcept {
    switch (ch) {
        case '[':
        case ']':
        case '(':
        case ')':
        case '{':
        case '}':
        case '|':
        case ';':
        case ',':
        case ':':
        case '=':
        case '>':
        case '<':
        case '-':
        case '#':
        case '&':
        case '@':
        case '!':
        case '+':
        case '*':
        case '/':
            return true;
        default:
            return false;
    }
}

char lower(char ch) noexcept {
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
}

bool equals_ignore_case(std::string_view lhs, std::string_view rhs) noexcept {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (lower(lhs[i]) != lower(rhs[i])) {
            return false;
        }
    }
    return true;
}

// 箭头表：按长度降序匹配（"==>" 先于 "=="、"-.->" 先于 "--"）。
//
// 表里收录了四个方言用到的全部箭杆，因为它们都是"长得像箭头的东西"，
// 归属词法层比让各方言自己拼 token 更不容易出错：
//   * 流程图：--> --- -.-> ==> <--> --x --o
//   * 类图  ：<|-- --|> <|.. ..|> *-- o-- ..>
//   * 序列图：-> ->> -->> -x --x -) --)
//   * DOT   ：-> --
// 代价：`o--`、`->` 这类写法在别的方言里若当标识符用会被切成箭头。评估过实际语料
// （mermaid/dot/plantuml/d2 的 identifier 不允许以 '-' 开头），方言解析器可自行纠偏。
constexpr std::array<std::string_view, 26> kArrowPatterns{
    "<-->", "-.->", "<|--", "--|>", "<|..", "..|>", "-->>", "--x", "--*",
    "--o",  "--)",  "==>",  "-->",  "===",  "---",  "-.-",  "..>", "->>",
    "<--",  "o--",  "*--",  "->",   "-x",   "-)",   "--",   "<-",
};

// 标识符内部遇到 '-' '.' 时要先看这里：`A-->B` 必须切成 A / --> / B，
// 而不是把 `A--` 当标识符。
bool arrow_starts_at(std::string_view source, std::size_t offset) noexcept {
    for (const std::string_view pattern : kArrowPatterns) {
        if (source.size() - offset >= pattern.size() &&
            source.compare(offset, pattern.size(), pattern) == 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

bool KeywordSet::contains(std::string_view word) const noexcept {
    for (const std::string_view candidate : words_) {
        if (equals_ignore_case(candidate, word)) {
            return true;
        }
    }
    return false;
}

Lexer::Lexer(std::string_view source, KeywordSet keywords, LexerOptions options)
    : source_(source), keywords_(keywords), options_(std::move(options)) {}

Token Lexer::next() noexcept {
    Token token;
    token.offset = offset_;
    token.line = line_;
    token.column = column_;

    // 空白 / 换行 / 注释：需要跳过的直接在这里消化，直到遇到实义字符。
    while (offset_ < source_.size()) {
        const char ch = source_[offset_];
        if (ch == ' ' || ch == '\t') {
            ++offset_;
            ++column_;
            continue;
        }
        if (ch == '\n' || ch == '\r') {
            std::size_t length = 1;
            if (ch == '\r' && offset_ + 1 < source_.size() && source_[offset_ + 1] == '\n') {
                length = 2;
            }
            token.offset = offset_;
            token.line = line_;
            token.column = column_;
            token.text.assign(source_.substr(offset_, length));
            token.length = length;
            offset_ += length;
            ++line_;
            column_ = 1;
            if (!options_.keep_newlines) {
                continue;
            }
            token.kind = TokenKind::Newline;
            return token;
        }
        bool is_comment = false;
        for (const std::string_view prefix : options_.comment_prefixes) {
            if (prefix.empty() || source_.size() - offset_ < prefix.size()) {
                continue;
            }
            if (source_.compare(offset_, prefix.size(), prefix) == 0) {
                is_comment = true;
                break;
            }
        }
        if (is_comment) {
            const std::size_t start = offset_;
            const std::size_t start_line = line_;
            const std::size_t start_column = column_;
            while (offset_ < source_.size() && source_[offset_] != '\n' &&
                   source_[offset_] != '\r') {
                ++offset_;
                ++column_;
            }
            if (!options_.keep_comments) {
                continue;
            }
            token.offset = start;
            token.line = start_line;
            token.column = start_column;
            token.text.assign(source_.substr(start, offset_ - start));
            token.length = token.text.size();
            token.kind = TokenKind::Comment;
            return token;
        }
        break;
    }

    if (offset_ >= source_.size()) {
        token.offset = offset_;
        token.line = line_;
        token.column = column_;
        token.kind = TokenKind::End;
        return token;
    }

    token.offset = offset_;
    token.line = line_;
    token.column = column_;
    const char ch = source_[offset_];

    // ── 字符串 ─────────────────────────────────────────────────────────
    if (ch == '"' || ch == '\'') {
        const char quote = ch;
        ++offset_;
        ++column_;
        std::string value;
        bool closed = false;
        while (offset_ < source_.size()) {
            const char current = source_[offset_];
            if (current == '\\' && offset_ + 1 < source_.size()) {
                value.push_back(source_[offset_ + 1]);
                offset_ += 2;
                column_ += 2;
                continue;
            }
            if (current == quote) {
                closed = true;
                ++offset_;
                ++column_;
                break;
            }
            if (current == '\n' || current == '\r') {
                break;  // 字符串不跨行：未闭合按 Invalid 处理
            }
            value.push_back(current);
            ++offset_;
            ++column_;
        }
        token.text = std::move(value);
        token.length = offset_ - token.offset;
        token.kind = closed ? TokenKind::Text : TokenKind::Invalid;
        return token;
    }

    // ── 箭头 ───────────────────────────────────────────────────────────
    if (arrow_starts_at(source_, offset_)) {
        for (const std::string_view pattern : kArrowPatterns) {
            if (source_.size() - offset_ >= pattern.size() &&
                source_.compare(offset_, pattern.size(), pattern) == 0) {
                token.kind = TokenKind::Arrow;
                token.text.assign(pattern);
                token.length = pattern.size();
                offset_ += pattern.size();
                column_ += pattern.size();
                return token;
            }
        }
    }

    // ── 标识符 / 关键字（ASCII 词字符 + 合法 UTF-8 序列） ──────────────
    if (is_identifier_start(ch) || static_cast<unsigned char>(ch) >= 0x80) {
        const std::size_t start = offset_;
        while (offset_ < source_.size()) {
            if (arrow_starts_at(source_, offset_)) {
                break;  // 让箭头独立成 token
            }
            const char current = source_[offset_];
            if (is_identifier_body(current)) {
                ++offset_;
                ++column_;
                continue;
            }
            if (static_cast<unsigned char>(current) >= 0x80) {
                const util::Utf8Decode decoded = util::decode(source_, offset_);
                if (!decoded.ok || decoded.consumed <= 0) {
                    break;
                }
                offset_ += static_cast<std::size_t>(decoded.consumed);
                column_ += static_cast<std::size_t>(decoded.consumed);
                continue;
            }
            break;
        }
        if (offset_ == start) {
            // 首个字节就是非法 UTF-8：产出 Invalid，并保证至少吃掉 1 字节。
            const util::Utf8Decode decoded = util::decode(source_, offset_);
            const std::size_t consumed =
                decoded.consumed > 0 ? static_cast<std::size_t>(decoded.consumed) : std::size_t{1};
            token.kind = TokenKind::Invalid;
            token.text.assign(source_.substr(offset_, consumed));
            token.length = consumed;
            offset_ += consumed;
            column_ += consumed;
            return token;
        }
        token.text.assign(source_.substr(start, offset_ - start));
        token.length = token.text.size();
        token.kind = keywords_.contains(token.text) ? TokenKind::Keyword : TokenKind::Identifier;
        return token;
    }

    // ── 数字 ───────────────────────────────────────────────────────────
    if (is_digit(ch)) {
        const std::size_t start = offset_;
        while (offset_ < source_.size() &&
               (is_digit(source_[offset_]) || source_[offset_] == '.')) {
            ++offset_;
            ++column_;
        }
        token.text.assign(source_.substr(start, offset_ - start));
        token.length = token.text.size();
        token.kind = TokenKind::Number;
        return token;
    }

    // ── 标点 ───────────────────────────────────────────────────────────
    if (is_punctuation(ch)) {
        token.kind = TokenKind::Punctuation;
        token.text.assign(1, ch);
        token.length = 1;
        ++offset_;
        ++column_;
        return token;
    }

    // ── 其它：Invalid，且**一定消耗至少 1 字节**（游标不许停住） ───────
    token.kind = TokenKind::Invalid;
    const util::Utf8Decode decoded = util::decode(source_, offset_);
    const std::size_t consumed =
        decoded.consumed > 0 ? static_cast<std::size_t>(decoded.consumed) : std::size_t{1};
    token.text.assign(source_.substr(offset_, consumed));
    token.length = consumed;
    offset_ += consumed;
    column_ += consumed;
    return token;
}

Result<std::vector<Token>> Lexer::tokenize() noexcept {
    std::vector<Token> tokens;
    while (true) {
        if (tokens.size() > options_.max_tokens) {
            return Unexpected<ErrorCode>(ErrorCode::OutOfRange);
        }
        Token token = next();
        const bool end = token.kind == TokenKind::End;
        tokens.push_back(std::move(token));
        if (end) {
            break;
        }
    }
    return tokens;
}

Result<std::vector<Token>> tokenize(std::string_view source, KeywordSet keywords,
                                    LexerOptions options) {
    Lexer lexer(source, keywords, std::move(options));
    return lexer.tokenize();
}

}  // namespace termchart::dsl
