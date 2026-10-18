// json.cpp —— JSON 解析实现（递归下降 + 自带扫描器）
//
// 自带扫描器而不复用 dsl/lexer.hpp：JSON 词法与 mermaid/dot 等方言差异过大
// （负号、科学计数法、\uXXXX 转义、无注释/换行语义），扩展 lexer 会波及所有
// 方言，收益不抵风险。ParseDiagnostic / ErrorCode 与 csv_sniff 同口径。
#include "termchart/dsl/json.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

namespace termchart::dsl {
namespace {

// Null 单例：at()/operator[] 越界与 find() 失败时返回它（const，调用方不得改）。
const JsonValue& null_value() noexcept {
    static const JsonValue kNull;
    return kNull;
}

bool is_ws(char ch) noexcept { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r'; }

bool is_digit(char ch) noexcept { return ch >= '0' && ch <= '9'; }

// 4 位十六进制 → 值；非 hex 返回 false。
bool hex4(std::string_view s, std::uint32_t& out) noexcept {
    if (s.size() < 4) {
        return false;
    }
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        const char ch = s[i];
        std::uint32_t digit = 0;
        if (ch >= '0' && ch <= '9') {
            digit = static_cast<std::uint32_t>(ch - '0');
        } else if (ch >= 'a' && ch <= 'f') {
            digit = static_cast<std::uint32_t>(ch - 'a' + 10);
        } else if (ch >= 'A' && ch <= 'F') {
            digit = static_cast<std::uint32_t>(ch - 'A' + 10);
        } else {
            return false;
        }
        value = (value << 4) | digit;
    }
    out = value;
    return true;
}

// Unicode 码点 → UTF-8 追加到 out。非法码点（代理区落单、> 0x10FFFF）返回 false。
bool append_utf8(std::string& out, std::uint32_t cp) noexcept {
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        return false;
    }
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return true;
}

class JsonParser {
public:
    JsonParser(std::string_view source, const JsonOptions& options)
        : src_(source), opts_(options) {}

    // 解析顶层值；尾部只允许空白。失败记诊断并返回 false。
    bool parse(JsonValue& out) {
        skip_ws();
        if (!parse_value(out)) {
            return false;
        }
        skip_ws();
        if (pos_ < src_.size()) {
            fail(ErrorCode::ParseError, "trailing characters after JSON value");
            return false;
        }
        return true;
    }

    [[nodiscard]] const ParseDiagnostic& diagnostic() const noexcept { return diag_; }

private:
    std::string_view src_;
    const JsonOptions& opts_;
    std::size_t pos_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
    std::size_t depth_ = 0;
    bool failed_ = false;
    ParseDiagnostic diag_;

    [[nodiscard]] bool at_end() const noexcept { return pos_ >= src_.size(); }
    [[nodiscard]] char cur() const noexcept { return src_[pos_]; }

    void advance() noexcept {
        if (at_end()) {
            return;
        }
        if (src_[pos_] == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
        ++pos_;
    }

    void skip_ws() noexcept {
        while (!at_end() && is_ws(cur())) {
            advance();
        }
    }

    void fail(ErrorCode code, std::string message) noexcept {
        if (failed_) {
            return;  // 只记第一个错误
        }
        failed_ = true;
        diag_.code = code;
        diag_.message = std::move(message);
        diag_.line = line_;
        diag_.column = column_;
        diag_.offset = pos_;
    }

    // 匹配字面量（true/false/null）；成功则前进。
    bool match_literal(std::string_view word) noexcept {
        if (src_.substr(pos_, word.size()) != word) {
            return false;
        }
        for (std::size_t i = 0; i < word.size(); ++i) {
            advance();
        }
        return true;
    }

    bool parse_value(JsonValue& out) {
        if (at_end()) {
            fail(ErrorCode::ParseError, "unexpected end of input");
            return false;
        }
        const char ch = cur();
        switch (ch) {
            case '{':
                return parse_object(out);
            case '[':
                return parse_array(out);
            case '"':
                return parse_string_value(out);
            case 't':
                if (match_literal("true")) {
                    out = JsonValue::boolean(true);
                    return true;
                }
                fail(ErrorCode::ParseError, "invalid literal (expected 'true')");
                return false;
            case 'f':
                if (match_literal("false")) {
                    out = JsonValue::boolean(false);
                    return true;
                }
                fail(ErrorCode::ParseError, "invalid literal (expected 'false')");
                return false;
            case 'n':
                if (match_literal("null")) {
                    out = JsonValue::null();
                    return true;
                }
                fail(ErrorCode::ParseError, "invalid literal (expected 'null')");
                return false;
            default:
                if (ch == '-' || is_digit(ch)) {
                    return parse_number(out);
                }
                fail(ErrorCode::ParseError, "unexpected character");
                return false;
        }
    }

    bool parse_object(JsonValue& out) {
        if (depth_ >= opts_.max_depth) {
            fail(ErrorCode::ParseError, "nesting depth exceeds limit");
            return false;
        }
        ++depth_;
        advance();  // '{'
        out = JsonValue::object();
        skip_ws();
        if (!at_end() && cur() == '}') {
            advance();
            --depth_;
            return true;
        }
        for (;;) {
            skip_ws();
            if (at_end() || cur() != '"') {
                fail(ErrorCode::ParseError, "expected object key string");
                --depth_;
                return false;
            }
            std::string key;
            if (!parse_string_raw(key)) {
                --depth_;
                return false;
            }
            skip_ws();
            if (at_end() || cur() != ':') {
                fail(ErrorCode::ParseError, "expected ':' after object key");
                --depth_;
                return false;
            }
            advance();
            skip_ws();
            JsonValue value;
            if (!parse_value(value)) {
                --depth_;
                return false;
            }
            out.set(std::move(key), std::move(value));
            skip_ws();
            if (at_end()) {
                fail(ErrorCode::ParseError, "unterminated object");
                --depth_;
                return false;
            }
            if (cur() == ',') {
                advance();
                continue;
            }
            if (cur() == '}') {
                advance();
                --depth_;
                return true;
            }
            fail(ErrorCode::ParseError, "expected ',' or '}' in object");
            --depth_;
            return false;
        }
    }

    bool parse_array(JsonValue& out) {
        if (depth_ >= opts_.max_depth) {
            fail(ErrorCode::ParseError, "nesting depth exceeds limit");
            return false;
        }
        ++depth_;
        advance();  // '['
        out = JsonValue::array();
        skip_ws();
        if (!at_end() && cur() == ']') {
            advance();
            --depth_;
            return true;
        }
        for (;;) {
            skip_ws();
            JsonValue value;
            if (!parse_value(value)) {
                --depth_;
                return false;
            }
            out.push(std::move(value));
            skip_ws();
            if (at_end()) {
                fail(ErrorCode::ParseError, "unterminated array");
                --depth_;
                return false;
            }
            if (cur() == ',') {
                advance();
                continue;
            }
            if (cur() == ']') {
                advance();
                --depth_;
                return true;
            }
            fail(ErrorCode::ParseError, "expected ',' or ']' in array");
            --depth_;
            return false;
        }
    }

    // 字符串值（含引号）→ JsonValue。
    bool parse_string_value(JsonValue& out) {
        std::string text;
        if (!parse_string_raw(text)) {
            return false;
        }
        out = JsonValue::string(std::move(text));
        return true;
    }

    // 字符串内容（引号已由调用方确认存在）。处理全部 JSON 转义。
    bool parse_string_raw(std::string& out) {
        advance();  // '"'
        for (;;) {
            if (at_end()) {
                fail(ErrorCode::ParseError, "unterminated string");
                return false;
            }
            const char ch = cur();
            if (ch == '"') {
                advance();
                return true;
            }
            if (static_cast<unsigned char>(ch) < 0x20) {
                fail(ErrorCode::ParseError, "unescaped control character in string");
                return false;
            }
            if (ch != '\\') {
                out.push_back(ch);
                advance();
                continue;
            }
            advance();  // '\\'
            if (at_end()) {
                fail(ErrorCode::ParseError, "unterminated escape sequence");
                return false;
            }
            const char esc = cur();
            switch (esc) {
                case '"': out.push_back('"'); advance(); break;
                case '\\': out.push_back('\\'); advance(); break;
                case '/': out.push_back('/'); advance(); break;
                case 'b': out.push_back('\b'); advance(); break;
                case 'f': out.push_back('\f'); advance(); break;
                case 'n': out.push_back('\n'); advance(); break;
                case 'r': out.push_back('\r'); advance(); break;
                case 't': out.push_back('\t'); advance(); break;
                case 'u': {
                    advance();  // 'u'
                    std::uint32_t cp = 0;
                    if (!hex4(src_.substr(pos_), cp)) {
                        fail(ErrorCode::ParseError, "invalid \\u escape");
                        return false;
                    }
                    for (int i = 0; i < 4; ++i) {
                        advance();
                    }
                    // 高代理：必须紧跟低代理 \uXXXX，组合成单个码点。
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (src_.substr(pos_, 2) != "\\u") {
                            fail(ErrorCode::ParseError, "lone high surrogate");
                            return false;
                        }
                        advance();
                        advance();  // "\u"
                        std::uint32_t low = 0;
                        if (!hex4(src_.substr(pos_), low)) {
                            fail(ErrorCode::ParseError, "invalid \\u escape");
                            return false;
                        }
                        for (int i = 0; i < 4; ++i) {
                            advance();
                        }
                        if (low < 0xDC00 || low > 0xDFFF) {
                            fail(ErrorCode::ParseError, "invalid low surrogate");
                            return false;
                        }
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        fail(ErrorCode::ParseError, "lone low surrogate");
                        return false;
                    }
                    if (!append_utf8(out, cp)) {
                        fail(ErrorCode::ParseError, "invalid unicode code point");
                        return false;
                    }
                    break;
                }
                default:
                    fail(ErrorCode::ParseError, "invalid escape character");
                    return false;
            }
        }
    }

    // 数字：-? int frac? exp?（JSON 规范：int 无前导 0，frac/exp 至少一位数字）。
    bool parse_number(JsonValue& out) {
        const std::size_t start = pos_;
        if (!at_end() && cur() == '-') {
            advance();
        }
        if (at_end() || !is_digit(cur())) {
            fail(ErrorCode::ParseError, "invalid number");
            return false;
        }
        if (cur() == '0') {
            advance();
        } else {
            while (!at_end() && is_digit(cur())) {
                advance();
            }
        }
        if (!at_end() && cur() == '.') {
            advance();
            if (at_end() || !is_digit(cur())) {
                fail(ErrorCode::ParseError, "invalid number (fraction has no digits)");
                return false;
            }
            while (!at_end() && is_digit(cur())) {
                advance();
            }
        }
        if (!at_end() && (cur() == 'e' || cur() == 'E')) {
            advance();
            if (!at_end() && (cur() == '+' || cur() == '-')) {
                advance();
            }
            if (at_end() || !is_digit(cur())) {
                fail(ErrorCode::ParseError, "invalid number (exponent has no digits)");
                return false;
            }
            while (!at_end() && is_digit(cur())) {
                advance();
            }
        }
        const std::string_view text = src_.substr(start, pos_ - start);
        // strtod 依赖 locale 的小数点；本库全程不调 setlocale，保持 "C" locale。
        // 扫描已保证格式合法，endptr 必指向末尾。
        char* end = nullptr;
        const double value = std::strtod(std::string(text).c_str(), &end);
        out = JsonValue::number(value);
        return true;
    }
};

}  // namespace

// ── JsonValue ────────────────────────────────────────────────────────────────

JsonValue JsonValue::null() noexcept { return JsonValue{}; }

JsonValue JsonValue::boolean(bool value) noexcept {
    JsonValue v;
    v.type_ = Type::Bool;
    v.bool_ = value;
    return v;
}

JsonValue JsonValue::number(double value) noexcept {
    JsonValue v;
    v.type_ = Type::Number;
    v.number_ = value;
    return v;
}

JsonValue JsonValue::string(std::string value) {
    JsonValue v;
    v.type_ = Type::String;
    v.string_ = std::move(value);
    return v;
}

JsonValue JsonValue::array() {
    JsonValue v;
    v.type_ = Type::Array;
    return v;
}

JsonValue JsonValue::object() {
    JsonValue v;
    v.type_ = Type::Object;
    return v;
}

bool JsonValue::as_bool(bool fallback) const noexcept {
    return type_ == Type::Bool ? bool_ : fallback;
}

double JsonValue::as_number(double fallback) const noexcept {
    return type_ == Type::Number ? number_ : fallback;
}

std::string_view JsonValue::as_string(std::string_view fallback) const noexcept {
    return type_ == Type::String ? std::string_view{string_} : fallback;
}

std::size_t JsonValue::size() const noexcept {
    if (type_ == Type::Array) {
        return array_.size();
    }
    if (type_ == Type::Object) {
        return object_.size();
    }
    return 0;
}

const JsonValue& JsonValue::at(std::size_t index) const noexcept {
    if (type_ != Type::Array || index >= array_.size()) {
        return null_value();
    }
    return array_[index];
}

const JsonValue& JsonValue::operator[](std::size_t index) const noexcept {
    return at(index);
}

const JsonValue* JsonValue::find(std::string_view key) const noexcept {
    if (type_ != Type::Object) {
        return nullptr;
    }
    for (const auto& [k, v] : object_) {
        if (k == key) {
            return &v;
        }
    }
    return nullptr;
}

const JsonValue& JsonValue::at(std::string_view key) const noexcept {
    const JsonValue* found = find(key);
    return found != nullptr ? *found : null_value();
}

void JsonValue::push(JsonValue value) {
    if (type_ == Type::Array) {
        array_.push_back(std::move(value));
    }
}

void JsonValue::set(std::string key, JsonValue value) {
    if (type_ != Type::Object) {
        return;
    }
    for (auto& [k, v] : object_) {
        if (k == key) {
            v = std::move(value);  // 同名键覆盖，保持原插入位置
            return;
        }
    }
    object_.emplace_back(std::move(key), std::move(value));
}

// ── parse_json ───────────────────────────────────────────────────────────────

Result<JsonValue> parse_json(std::string_view source, const JsonOptions& options,
                             ParseDiagnostic* out_diagnostic) {
    if (source.size() > options.max_bytes) {
        if (out_diagnostic != nullptr) {
            out_diagnostic->code = ErrorCode::OutOfRange;
            out_diagnostic->message = "input exceeds max_bytes";
            out_diagnostic->offset = options.max_bytes;
        }
        return Unexpected<ErrorCode>(ErrorCode::OutOfRange);
    }
    JsonParser parser(source, options);
    JsonValue value;
    if (!parser.parse(value)) {
        if (out_diagnostic != nullptr) {
            *out_diagnostic = parser.diagnostic();
        }
        return Unexpected<ErrorCode>(parser.diagnostic().code);
    }
    return value;
}

}  // namespace termchart::dsl
