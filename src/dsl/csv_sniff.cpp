#include "termchart/dsl/csv_sniff.hpp"

#include <algorithm>
#include <cstddef>
#include <istream>
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

// ── 流式字符源 ────────────────────────────────────────────────────────────
//
// CSV 的字段语义（引号、"" 转义、宽松尾部、跳过空行）必须与整表路径**完全一致**，
// 所以这里实现一个"能 peek 的字符流"，并把字段读取逻辑按同样的规则再写一遍
// （read_field_stream）。两条路径的等价性由 tests/unit/dsl/test_dsl_csv.cpp
// 里的差分测试守住：同一份输入，parse_csv 与 parse_csv_stream 必须逐字段一致。

// 引号起始位置（诊断用）：偏移 + 行号 + 列号。
struct QuotePosition {
    std::size_t offset = 0;
    std::size_t line = 1;
    std::size_t column = 1;
};

// 带缓冲的流字符源：istream → 可 peek 的字符流。
//
// 缓冲是**滑动窗口**：refill 时把未读的尾巴挪到队首再追加新数据，
// 因此可以安全地"预览后面 N 个字节"（BOM 探测要用）而不消费它们。
// （早期版本用 peek + pushback 做 BOM 探测，结果那 3 个字节既没被消费
//  又被塞回队列，等于读了两遍——字段错位、后续解析崩。）
class StreamSource {
public:
    explicit StreamSource(std::istream& input) : input_(input) {}

    bool peek(char& out) {
        if (pos_ >= buffer_.size() && !refill()) {
            return false;
        }
        out = buffer_[pos_];
        return true;
    }

    bool get(char& out) {
        if (!peek(out)) {
            return false;
        }
        ++pos_;
        track_line(out);
        return true;
    }

    // 预览接下来的 n 个字节（**不消费**）；返回实际能预览到的字节数。
    std::size_t peek_bytes(char* out, std::size_t n) {
        while (buffer_.size() - pos_ < n && refill()) {
        }
        const std::size_t available = buffer_.size() - pos_;
        const std::size_t count = std::min(n, available);
        for (std::size_t i = 0; i < count; ++i) {
            out[i] = buffer_[pos_ + i];
        }
        return count;
    }

    // 消费 n 个字节（用于跳过 BOM）。
    void skip(std::size_t n) {
        char ch = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (!get(ch)) {
                return;
            }
        }
    }

    // 已消费字节数（诊断定位用）。
    std::size_t position() const noexcept { return base_ + pos_; }

    // 当前光标所在位置的行号/列号（1 起）。
    void line_column(std::size_t& line, std::size_t& column) const noexcept {
        line = line_;
        column = position() >= line_start_ ? position() - line_start_ + 1 : 1;
    }

private:
    static constexpr std::size_t kChunkSize = 64 * 1024;

    // 滑动窗口补货：把未读尾巴挪到队首，再从流里追加一块。
    bool refill() {
        const std::size_t remaining = buffer_.size() - pos_;
        for (std::size_t i = 0; i < remaining; ++i) {
            buffer_[i] = buffer_[pos_ + i];
        }
        base_ += pos_;
        pos_ = 0;
        buffer_.resize(remaining);
        const std::size_t head = remaining;
        buffer_.resize(head + kChunkSize);
        input_.read(buffer_.data() + head, static_cast<std::streamsize>(kChunkSize));
        const std::streamsize got = input_.gcount();
        buffer_.resize(head + (got > 0 ? static_cast<std::size_t>(got) : 0));
        return got > 0;
    }

    // 增量统计行号：\n 换行；单独的 \r 也算换行（\r\n 按一次算）。
    void track_line(char ch) {
        const std::size_t absolute = base_ + pos_;
        if (ch == '\n') {
            ++line_;
            line_start_ = absolute;
            pending_cr_ = false;
        } else if (ch == '\r') {
            pending_cr_ = true;
        } else if (pending_cr_) {
            pending_cr_ = false;
            ++line_;
            line_start_ = absolute - 1;
        }
    }

    std::istream& input_;
    std::vector<char> buffer_;
    std::size_t pos_ = 0;
    std::size_t base_ = 0;
    std::size_t line_ = 1;
    std::size_t line_start_ = 0;
    bool pending_cr_ = false;
};

// 流式版字段读取：与 read_field 逐条规则一致。
FieldEnd read_field_stream(StreamSource& src, char delimiter, std::string& out, bool& unterminated,
                           QuotePosition& quote) {
    out.clear();
    unterminated = false;
    char ch = 0;
    if (!src.peek(ch)) {
        return FieldEnd::Eof;
    }
    if (ch == '"') {
        quote.offset = src.position();
        src.line_column(quote.line, quote.column);
        (void)src.get(ch);
        bool closed = false;
        while (src.get(ch)) {
            if (ch == '"') {
                char next = 0;
                if (src.peek(next) && next == '"') {
                    out.push_back('"');  // "" → 一个字面引号
                    (void)src.get(next);
                    continue;
                }
                closed = true;
                break;
            }
            out.push_back(ch);  // 引号内分隔符与换行都是字面内容
        }
        if (!closed) {
            unterminated = true;
            return FieldEnd::Eof;
        }
    } else {
        while (src.peek(ch) && ch != delimiter && ch != '\n' && ch != '\r') {
            out.push_back(ch);
            (void)src.get(ch);
        }
    }
    // 闭合引号之后的剩余字符按字面并入字段（与整表路径一致）。
    while (src.peek(ch) && ch != delimiter && ch != '\n' && ch != '\r') {
        out.push_back(ch);
        (void)src.get(ch);
    }
    if (!src.peek(ch)) {
        return FieldEnd::Eof;
    }
    return ch == delimiter ? FieldEnd::Delimiter : FieldEnd::Newline;
}

// 流式版读一行：跳过完全空白的行；返回 false 表示流结束。
bool read_row_stream(StreamSource& src, char delimiter, std::vector<std::string>& fields,
                      bool& unterminated, QuotePosition& quote) {
    unterminated = false;
    char ch = 0;
    while (src.peek(ch) && (ch == '\n' || ch == '\r')) {  // 跳过完全空白的行
        (void)src.get(ch);
        if (ch == '\r') {
            char next = 0;
            if (src.peek(next) && next == '\n') {
                (void)src.get(next);
            }
        }
    }
    if (!src.peek(ch)) {
        return false;
    }
    fields.clear();
    while (true) {
        std::string field;
        bool field_unterminated = false;
        QuotePosition field_quote;
        const FieldEnd end =
            read_field_stream(src, delimiter, field, field_unterminated, field_quote);
        if (field_unterminated) {
            unterminated = true;
            quote = field_quote;
            return true;
        }
        fields.push_back(std::move(field));
        if (end == FieldEnd::Delimiter) {
            // 必须消费掉分隔符再进下一轮：read_field_stream 与整表路径的 read_field 一样，
            // 读到分隔符只 peek 不消费（由调用方跳过）。少了这一行，下一轮又会 peek 到
            // 同一个分隔符 → 空字段 → 又是 Delimiter → 无限循环（fields 无限增长 →
            // std::bad_alloc）。整表路径对应的就是 `++pos; // 跳过分隔符`。
            (void)src.get(ch);
            continue;
        }
        if (end == FieldEnd::Newline) {
            (void)src.get(ch);
            if (ch == '\r') {
                char next = 0;
                if (src.peek(next) && next == '\n') {
                    (void)src.get(next);
                }
            }
        }
        break;
    }
    return true;
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

// 读入行（跳过空行、处理 BOM、按需截断），失败时经 out_diagnostic 报错。
//
// limit：最多读多少行（含表头）。
// overflow_is_error：true = 读满了 limit 还有下一行 → 报 OutOfRange；
//                    false = "采样到此为止"（sniff 用），不是错误。
bool load_rows(std::string_view text, const CsvOptions& options, RawCsv& raw,
               std::string_view& stripped, ParseDiagnostic* out_diagnostic, ErrorCode& code,
               std::size_t limit, bool overflow_is_error) {
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
    raw = read_all_rows(stripped, options.delimiter, limit);
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
    if (raw.overflow && overflow_is_error) {
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
    const std::size_t header_rows = options.has_header ? 1U : 0U;
    const std::size_t row_limit = saturating_add(options.max_rows, header_rows);
    if (!load_rows(source, options, raw, stripped, out_diagnostic, code, row_limit, true)) {
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
    const std::size_t header_rows = options.has_header ? 1U : 0U;
    // 关键：只读**样本**行就停手。历史上这里读的是 max_rows（默认 10000）行，
    // 也就是"为了推断 100 行的类型，把整个文件读进内存"。sniff_rows==0
    // 才表示"用全部数据行"，那时才需要读满并保留超限报错。
    const bool sample_all = options.sniff_rows == 0;
    const std::size_t sample_rows =
        sample_all ? options.max_rows : std::min(options.sniff_rows, options.max_rows);
    const std::size_t row_limit = saturating_add(sample_rows, header_rows);
    // 推断是"尽力而为"的采样，不是整表物化：读满样本行还有下一行**不是错误**
    // （否则限定 sniff_rows 时反而会因为文件更长而失败）。超限报错只属于 parse_csv。
    if (!load_rows(source, options, raw, stripped, out_diagnostic, code, row_limit, false)) {
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

Status parse_csv_stream(std::istream& input, const CsvOptions& options, CsvRowSink& sink,
                        ParseDiagnostic* out_diagnostic) {
    if (!is_valid_delimiter(options.delimiter)) {
        if (out_diagnostic != nullptr) {
            out_diagnostic->code = ErrorCode::InvalidArgument;
            out_diagnostic->message = "delimiter 不能是引号/换行/NUL";
            out_diagnostic->offset = 0;
            out_diagnostic->line = 1;
            out_diagnostic->column = 1;
        }
        return ErrorCode::InvalidArgument;
    }

    StreamSource source(input);
    // BOM：用"不消费地预览 3 字节"判断，是 BOM 才跳过。
    // （不能用 peek + 退回：那 3 个字节既没被消费又被放回，等于读了两遍。）
    {
        char probe[3] = {0, 0, 0};
        const std::size_t got = source.peek_bytes(probe, 3);
        const bool is_bom = got == 3 && static_cast<unsigned char>(probe[0]) == 0xEF &&
                            static_cast<unsigned char>(probe[1]) == 0xBB &&
                            static_cast<unsigned char>(probe[2]) == 0xBF;
        if (is_bom) {
            source.skip(3);
        }
    }

    const std::size_t header_rows = options.has_header ? 1U : 0U;
    const std::size_t row_limit = saturating_add(options.max_rows, header_rows);
    std::size_t rows_read = 0;
    std::vector<std::string> fields;
    bool unterminated = false;
    QuotePosition quote_position;
    while (read_row_stream(source, options.delimiter, fields, unterminated, quote_position)) {
        if (unterminated) {
            if (out_diagnostic != nullptr) {
                out_diagnostic->code = ErrorCode::ParseError;
                out_diagnostic->message = "引号字段未闭合";
                // 用流式读取时记下的**引号起点**位置，而不是报错时刻的位置——此时流已走到
                // 行尾，取当前位置会指错地方。
                out_diagnostic->offset = quote_position.offset;
                out_diagnostic->line = quote_position.line;
                out_diagnostic->column = quote_position.column;
            }
            return ErrorCode::ParseError;
        }
        if (rows_read >= row_limit) {
            if (out_diagnostic != nullptr) {
                out_diagnostic->code = ErrorCode::OutOfRange;
                out_diagnostic->message = "数据行超过 max_rows";
                out_diagnostic->offset = source.position();
                source.line_column(out_diagnostic->line, out_diagnostic->column);
            }
            return ErrorCode::OutOfRange;
        }
        ++rows_read;
        if (!sink.on_row(fields)) {
            break;  // 回调要求提前结束（不是错误）
        }
    }
    return ErrorCode::Ok;
}

}  // namespace termchart::dsl
