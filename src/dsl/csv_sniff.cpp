#include "termchart/dsl/csv_sniff.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace termchart::dsl {
namespace {

constexpr std::size_t kNoLimit = std::numeric_limits<std::size_t>::max();

bool is_blank(char ch) noexcept { return ch == ' ' || ch == '\t'; }

std::string_view trim(std::string_view text) noexcept {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && is_blank(text[begin])) {
        ++begin;
    }
    while (end > begin && is_blank(text[end - 1])) {
        --end;
    }
    return text.substr(begin, end - begin);
}

bool is_digit(char ch) noexcept { return ch >= '0' && ch <= '9'; }

char lower(char ch) noexcept {
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
}

bool iequals(std::string_view lhs, std::string_view rhs) noexcept {
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

bool is_bool(std::string_view s) noexcept {
    return iequals(s, "true") || iequals(s, "false") || iequals(s, "yes") ||
           iequals(s, "no") || s == "0" || s == "1";
}

bool is_integer(std::string_view s) noexcept {
    if (s.empty()) {
        return false;
    }
    std::size_t i = 0;
    if (s[i] == '+' || s[i] == '-') {
        ++i;
    }
    if (i >= s.size()) {
        return false;
    }
    for (; i < s.size(); ++i) {
        if (!is_digit(s[i])) {
            return false;
        }
    }
    return true;
}

bool is_real(std::string_view s) noexcept {
    if (s.empty()) {
        return false;
    }
    std::size_t i = 0;
    if (s[i] == '+' || s[i] == '-') {
        ++i;
    }
    std::size_t digits = 0;
    while (i < s.size() && is_digit(s[i])) {
        ++i;
        ++digits;
    }
    if (i < s.size() && s[i] == '.') {
        ++i;
        while (i < s.size() && is_digit(s[i])) {
            ++i;
            ++digits;
        }
    }
    if (digits == 0) {
        return false;
    }
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
            ++i;
        }
        std::size_t exp_digits = 0;
        while (i < s.size() && is_digit(s[i])) {
            ++i;
            ++exp_digits;
        }
        if (exp_digits == 0) {
            return false;
        }
    }
    return i == s.size();
}

bool is_digits(std::string_view s, std::size_t pos, std::size_t count) noexcept {
    if (pos + count > s.size()) {
        return false;
    }
    for (std::size_t k = 0; k < count; ++k) {
        if (!is_digit(s[pos + k])) {
            return false;
        }
    }
    return true;
}

int two_digit_value(std::string_view s, std::size_t pos) noexcept {
    return (s[pos] - '0') * 10 + (s[pos + 1] - '0');
}

bool is_date(std::string_view s) noexcept {
    if (s.size() < 10 || !is_digits(s, 0, 4) || s[4] != '-' || !is_digits(s, 5, 2) ||
        s[7] != '-' || !is_digits(s, 8, 2)) {
        return false;
    }
    const int month = two_digit_value(s, 5);
    const int day = two_digit_value(s, 8);
    if (month < 1 || month > 12 || day < 1 || day > 31) {
        return false;
    }
    if (s.size() == 10) {
        return true;
    }
    std::size_t i = 10;
    if (s[i] != ' ' && s[i] != 'T') {
        return false;
    }
    ++i;
    if (!is_digits(s, i, 2) || s[i + 2] != ':' || !is_digits(s, i + 3, 2)) {
        return false;
    }
    const int hour = two_digit_value(s, i);
    const int minute = two_digit_value(s, i + 3);
    if (hour > 23 || minute > 59) {
        return false;
    }
    const std::size_t after_minute = i + 5;
    if (after_minute == s.size()) {
        return true;
    }
    if (s[after_minute] != ':') {
        return false;
    }
    if (!is_digits(s, after_minute + 1, 2)) {
        return false;
    }
    return after_minute + 3 == s.size() && two_digit_value(s, after_minute + 1) <= 59;
}

enum class FieldEnd { Delimiter, Newline, Eof };

FieldEnd read_field(std::string_view src, std::size_t& pos, char delimiter, std::string& out,
                    bool& unterminated, std::size_t& quote_offset) {
    out.clear();
    unterminated = false;
    if (pos < src.size() && src[pos] == '"') {
        quote_offset = pos;
        ++pos;
        bool closed = false;
        while (pos < src.size()) {
            const char ch = src[pos];
            if (ch == '"') {
                if (pos + 1 < src.size() && src[pos + 1] == '"') {
                    out.push_back('"');  // "" → 一个字面引号
                    pos += 2;
                    continue;
                }
                ++pos;
                closed = true;
                break;
            }
            out.push_back(ch);  // 引号内分隔符与换行都是字面内容
            ++pos;
        }
        if (!closed) {
            unterminated = true;
            return FieldEnd::Eof;
        }
    } else {
        while (pos < src.size() && src[pos] != delimiter && src[pos] != '\n' && src[pos] != '\r') {
            out.push_back(src[pos]);
            ++pos;
        }
    }
    // 闭合引号之后的剩余字符（到分隔符/行尾为止）按字面并入字段，宽松不报错。
    while (pos < src.size() && src[pos] != delimiter && src[pos] != '\n' && src[pos] != '\r') {
        out.push_back(src[pos]);
        ++pos;
    }
    if (pos >= src.size()) {
        return FieldEnd::Eof;
    }
    return src[pos] == delimiter ? FieldEnd::Delimiter : FieldEnd::Newline;
}

struct RawCsv {
    std::vector<std::vector<std::string>> rows;
    bool unterminated = false;
    std::size_t quote_offset = 0;
    bool overflow = false;
};

void consume_newline(std::string_view src, std::size_t& pos) noexcept {
    if (src[pos] == '\r' && pos + 1 < src.size() && src[pos + 1] == '\n') {
        pos += 2;
    } else {
        ++pos;
    }
}

RawCsv read_all_rows(std::string_view src, char delimiter, std::size_t row_limit) {
    RawCsv result;
    std::size_t pos = 0;
    while (pos < src.size()) {
        if (src[pos] == '\n' || src[pos] == '\r') {
            consume_newline(src, pos);  // 跳过完全空白的行
            continue;
        }
        if (result.rows.size() >= row_limit) {
            result.overflow = true;
            return result;
        }
        std::vector<std::string> row;
        while (true) {
            std::string field;
            bool unterminated = false;
            std::size_t quote_offset = 0;
            const FieldEnd end = read_field(src, pos, delimiter, field, unterminated, quote_offset);
            row.push_back(std::move(field));
            if (unterminated) {
                result.unterminated = true;
                result.quote_offset = quote_offset;
                return result;
            }
            if (end == FieldEnd::Delimiter) {
                ++pos;  // 跳过分隔符
                continue;
            }
            if (end == FieldEnd::Newline) {
                consume_newline(src, pos);
            }
            break;  // Newline 或 Eof 结束本行
        }
        result.rows.push_back(std::move(row));
    }
    return result;
}

bool is_valid_delimiter(char ch) noexcept {
    return ch != '"' && ch != '\n' && ch != '\r' && ch != '\0';
}

std::size_t saturating_add(std::size_t a, std::size_t b) noexcept {
    return (a > kNoLimit - b) ? kNoLimit : a + b;
}

void position_of(std::string_view source, std::size_t offset, std::size_t& line,
                 std::size_t& column) noexcept {
    line = 1;
    column = 1;
    const std::size_t limit = std::min(offset, source.size());
    for (std::size_t i = 0; i < limit; ++i) {
        if (source[i] == '\n') {
            ++line;
            column = 1;
        } else if (source[i] == '\r') {
            if (i + 1 < limit && source[i + 1] == '\n') {
                continue;
            }
            ++line;
            column = 1;
        } else {
            ++column;
        }
    }
}

std::string_view strip_bom(std::string_view source) noexcept {
    if (source.size() >= 3 && static_cast<unsigned char>(source[0]) == 0xEF &&
        static_cast<unsigned char>(source[1]) == 0xBB &&
        static_cast<unsigned char>(source[2]) == 0xBF) {
        return source.substr(3);
    }
    return source;
}

// 列类型累积器：所有非空样本都满足某类型，才允许把该类型作为结论。
struct TypeAccumulator {
    bool all_bool = true;
    bool all_int = true;
    bool all_real = true;
    bool all_date = true;
    std::size_t filled = 0;
    std::size_t empty = 0;

    void add(std::string_view raw) noexcept {
        const std::string_view value = trim(raw);
        if (value.empty()) {
            ++empty;
            return;
        }
        ++filled;
        all_bool = all_bool && is_bool(value);
        all_int = all_int && is_integer(value);
        all_real = all_real && is_real(value);
        all_date = all_date && is_date(value);
    }

    [[nodiscard]] ColumnType resolve() const noexcept {
        if (filled == 0) {
            return ColumnType::Empty;
        }
        if (all_bool) {
            return ColumnType::Boolean;
        }
        if (all_int) {
            return ColumnType::Integer;
        }
        if (all_real) {
            return ColumnType::Real;
        }
        if (all_date) {
            return ColumnType::Date;
        }
        return ColumnType::Text;
    }
};

// 读入全部行（跳过空行、处理 BOM、按需截断），失败时经 out_diagnostic 报错。
bool load_rows(std::string_view text, const CsvOptions& options, RawCsv& raw,
               std::string_view& stripped, ParseDiagnostic* out_diagnostic, ErrorCode& code) {
    stripped = strip_bom(text);
    if (!is_valid_delimiter(options.delimiter)) {
        code = ErrorCode::InvalidArgument;
        if (out_diagnostic != nullptr) {
            out_diagnostic->code = code;
            out_diagnostic->message = "delimiter 不能是引号/换行/NUL";
            out_diagnostic->offset = 0;
            out_diagnostic->line = 1;
            out_diagnostic->column = 1;
        }
        return false;
    }
    const std::size_t header_rows = options.has_header ? 1U : 0U;
    const std::size_t row_limit = saturating_add(options.max_rows, header_rows);
    raw = read_all_rows(stripped, options.delimiter, row_limit);
    if (raw.unterminated) {
        code = ErrorCode::ParseError;
        if (out_diagnostic != nullptr) {
            out_diagnostic->code = code;
            out_diagnostic->message = "引号字段未闭合";
            out_diagnostic->offset = raw.quote_offset;
            position_of(stripped, raw.quote_offset, out_diagnostic->line, out_diagnostic->column);
        }
        return false;
    }
    if (raw.overflow) {
        code = ErrorCode::OutOfRange;
        if (out_diagnostic != nullptr) {
            out_diagnostic->code = code;
            out_diagnostic->message = "数据行超过 max_rows";
            out_diagnostic->offset = stripped.size();
            position_of(stripped, stripped.size(), out_diagnostic->line, out_diagnostic->column);
        }
        return false;
    }
    return true;
}

}  // namespace

Result<document::TableModel> parse_csv(std::string_view source, const CsvOptions& options,
                                       ParseDiagnostic* out_diagnostic) {
    RawCsv raw;
    std::string_view stripped;
    ErrorCode code = ErrorCode::Ok;
    if (!load_rows(source, options, raw, stripped, out_diagnostic, code)) {
        return Unexpected<ErrorCode>(code);
    }

    document::TableModel model;
    std::size_t begin = 0;
    if (options.has_header && !raw.rows.empty()) {
        model.set_header(std::move(raw.rows.front()));
        begin = 1;
    }
    for (std::size_t i = begin; i < raw.rows.size(); ++i) {
        model.add_row(std::move(raw.rows[i]));
    }
    return model;
}

Result<std::vector<ColumnProfile>> sniff_columns(std::string_view source,
                                                 const CsvOptions& options,
                                                 ParseDiagnostic* out_diagnostic) {
    RawCsv raw;
    std::string_view stripped;
    ErrorCode code = ErrorCode::Ok;
    if (!load_rows(source, options, raw, stripped, out_diagnostic, code)) {
        return Unexpected<ErrorCode>(code);
    }

    std::vector<std::string> names;
    std::size_t data_begin = 0;
    if (options.has_header && !raw.rows.empty()) {
        names = raw.rows.front();
        data_begin = 1;
    } else {
        std::size_t column_count = 0;
        for (const std::vector<std::string>& row : raw.rows) {
            column_count = std::max(column_count, row.size());
        }
        names.reserve(column_count);
        for (std::size_t c = 0; c < column_count; ++c) {
            names.push_back("col" + std::to_string(c + 1));
        }
    }

    std::size_t sample_end = raw.rows.size();
    if (options.sniff_rows != 0) {
        sample_end = std::min(sample_end, saturating_add(data_begin, options.sniff_rows));
    }

    std::vector<ColumnProfile> profiles;
    profiles.reserve(names.size());
    for (std::size_t c = 0; c < names.size(); ++c) {
        ColumnProfile profile;
        profile.name = names[c];
        TypeAccumulator accumulator;
        for (std::size_t r = data_begin; r < sample_end; ++r) {
            const std::vector<std::string>& row = raw.rows[r];
            accumulator.add(c < row.size() ? std::string_view(row[c]) : std::string_view{});
        }
        profile.type = accumulator.resolve();
        profile.filled = accumulator.filled;
        profile.empty = accumulator.empty;
        profiles.push_back(std::move(profile));
    }
    return profiles;
}

}  // namespace termchart::dsl
