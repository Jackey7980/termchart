#include "termchart/io/csv.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace termchart::io {
namespace {

// UTF-8 BOM（3 字节）：首列名不带它。
constexpr std::string_view kBom = "\xEF\xBB\xBF";

bool starts_with_bom(std::string_view s) noexcept {
    return s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF &&
           static_cast<unsigned char>(s[1]) == 0xBB && static_cast<unsigned char>(s[2]) == 0xBF;
}

// 引号外的候选分隔符计数（前 20 行内）。
char sniff_delimiter(std::string_view src, std::size_t start) noexcept {
    constexpr char kCandidates[] = {',', ';', '\t', '|'};
    std::size_t counts[4] = {0, 0, 0, 0};
    std::size_t lines = 0;
    bool in_quotes = false;
    for (std::size_t i = start; i < src.size() && lines < 20; ++i) {
        const char ch = src[i];
        if (ch == '"') {
            in_quotes = !in_quotes;
        } else if (!in_quotes) {
            if (ch == '\n') {
                ++lines;
                continue;
            }
            for (int c = 0; c < 4; ++c) {
                if (ch == kCandidates[c]) {
                    ++counts[c];
                    break;
                }
            }
        }
    }
    // 并列时按候选顺序取先者（, 优先于 ; \t |）。
    std::size_t best = 0;
    for (int c = 1; c < 4; ++c) {
        if (counts[c] > counts[best]) best = static_cast<std::size_t>(c);
    }
    return counts[best] > 0 ? kCandidates[best] : ',';
}

void trim(std::string_view& s) noexcept {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
}

// 字段 → double。空/NA 族 → NaN（返回 true）。非数字 → false（调用方报诊断）。
bool field_to_double(std::string_view f, double& out) noexcept {
    trim(f);
    if (f.empty()) {
        out = std::nan("");
        return true;
    }
    if (f.size() <= 3) {  // NaN / NA 族（大小写不敏感）
        std::string upper;
        upper.reserve(f.size());
        for (const char ch : f) {
            upper.push_back(static_cast<char>((ch >= 'a' && ch <= 'z') ? ch - 'a' + 'A' : ch));
        }
        if (upper == "NAN" || upper == "NA") {
            out = std::nan("");
            return true;
        }
    }
    const char* first = f.data();
    const char* last = f.data() + f.size();
    double value = 0.0;
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc() || ptr != last) return false;
    out = value;
    return true;
}

// 解析一行字段（不含行尾）。fields 中的视图在 source 存活期内有效；
// 含 "" 转义的引号字段退化为 owned_escape（一次分配）。
// 返回 false 时 diag 已填（未闭合引号）。
bool parse_row(std::string_view source, std::size_t& pos, char delim,
               std::vector<std::string_view>& fields, std::string& owned_escape,
               std::size_t line_no, ParseDiagnostic* diag) noexcept {
    fields.clear();
    while (true) {
        std::string_view field;
        if (pos < source.size() && source[pos] == '"') {
            const std::size_t quote_at = pos;
            ++pos;
            owned_escape.clear();
            bool escaped = false;  // 本字段是否出现过 "" 转义（需 owned 缓冲）
            std::size_t plain_start = pos;
            bool closed = false;
            while (pos < source.size()) {
                const char ch = source[pos];
                if (ch == '"') {
                    if (pos + 1 < source.size() && source[pos + 1] == '"') {
                        if (!escaped) {
                            owned_escape.assign(source.substr(plain_start, pos - plain_start));
                            escaped = true;
                        } else {
                            owned_escape.append(source.substr(plain_start, pos - plain_start));
                        }
                        owned_escape.push_back('"');
                        pos += 2;
                        plain_start = pos;
                        continue;
                    }
                    ++pos;  // 吃掉收尾引号
                    closed = true;
                    break;
                }
                ++pos;
            }
            if (!closed) {
                if (diag) {
                    diag->code = ErrorCode::ParseError;
                    diag->line = line_no;
                    diag->column = quote_at + 1;
                    diag->offset = quote_at;
                    diag->message = "CSV: 引号字段未闭合";
                }
                return false;
            }
            if (escaped) {
                owned_escape.append(source.substr(plain_start, pos - 1 - plain_start));
                field = std::string_view(owned_escape);
            } else {
                field = source.substr(plain_start, pos - 1 - plain_start);
            }
            // 闭合引号后到分隔符/行尾的剩余字符按宽松口径忽略。
            while (pos < source.size() && source[pos] != delim && source[pos] != '\n' &&
                   source[pos] != '\r') {
                ++pos;
            }
        } else {
            const std::size_t start = pos;
            while (pos < source.size() && source[pos] != delim && source[pos] != '\n' &&
                   source[pos] != '\r') {
                ++pos;
            }
            field = source.substr(start, pos - start);
        }
        fields.push_back(field);
        if (pos < source.size() && source[pos] == delim) {
            ++pos;
            continue;  // 下一个字段
        }
        break;  // 行尾
    }
    // 吃掉行尾（LF / CRLF / CR）。
    if (pos < source.size() && source[pos] == '\r') ++pos;
    if (pos < source.size() && source[pos] == '\n') ++pos;
    return true;
}

}  // namespace

Result<CsvTable> parse_csv(std::string_view source, const CsvOptions& options,
                           ParseDiagnostic* out_diagnostic) {
    CsvTable table;
    std::size_t pos = 0;
    if (starts_with_bom(source)) pos = kBom.size();

    const char delim = options.delimiter == 0 ? sniff_delimiter(source, pos) : options.delimiter;

    std::vector<std::string_view> fields;
    std::string owned_escape;
    std::size_t line_no = 1;
    bool header_done = false;

    while (pos < source.size()) {
        // 注释行（允许前导空白）
        std::size_t line_start = pos;
        while (line_start < source.size() &&
               (source[line_start] == ' ' || source[line_start] == '\t')) {
            ++line_start;
        }
        if (options.comment != 0 && line_start < source.size() &&
            source[line_start] == options.comment) {
            while (pos < source.size() && source[pos] != '\n') ++pos;
            if (pos < source.size()) ++pos;
            ++line_no;
            continue;
        }
        if (!parse_row(source, pos, delim, fields, owned_escape, line_no, out_diagnostic)) {
            return Unexpected<ErrorCode>(ErrorCode::ParseError);
        }
        ++line_no;

        if (!header_done && options.has_header) {
            table.headers.reserve(fields.size());
            for (const std::string_view f : fields) {
                table.headers.emplace_back(f);
            }
            header_done = true;
            continue;
        }
        if (table.columns.empty()) {
            // 首数据行定型列数
            table.columns.resize(table.headers.empty() ? fields.size() : table.headers.size());
            if (!table.headers.empty() && fields.size() != table.headers.size()) {
                if (out_diagnostic) {
                    out_diagnostic->code = ErrorCode::InvalidArgument;
                    out_diagnostic->line = line_no - 1;
                    out_diagnostic->message = "CSV: 数据行列数与表头不一致";
                }
                return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
            }
        }
        if (fields.size() != table.columns.size()) {
            if (out_diagnostic) {
                out_diagnostic->code = ErrorCode::InvalidArgument;
                out_diagnostic->line = line_no - 1;
                out_diagnostic->message = "CSV: 行列数不一致";
            }
            return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
        }
        double value = 0.0;
        for (std::size_t c = 0; c < fields.size(); ++c) {
            if (!field_to_double(fields[c], value)) {
                if (out_diagnostic) {
                    out_diagnostic->code = ErrorCode::ParseError;
                    out_diagnostic->line = line_no - 1;
                    out_diagnostic->column = c + 1;
                    out_diagnostic->message = "CSV: 非数字字段（数值入口请用 dsl/csv_sniff）";
                }
                return Unexpected<ErrorCode>(ErrorCode::ParseError);
            }
            table.columns[c].push_back(value);
        }
        ++table.row_count;
    }

    if (table.headers.empty() && !options.has_header) {
        // has_header=false：列名 col0..colN-1 按实际列数生成；无数据行则留空。
        for (std::size_t c = 0; c < table.columns.size(); ++c) {
            table.headers.push_back("col" + std::to_string(c));
        }
    }
    return Result<CsvTable>(std::move(table));
}

}  // namespace termchart::io
