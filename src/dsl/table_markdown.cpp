#include "termchart/dsl/table_markdown.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace termchart::dsl {
namespace {

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

std::string trim_cell(std::string_view text) {
    const std::string_view trimmed = trim(text);
    return std::string(trimmed);
}

// 按行切分；\n、\r、\r\n 都算行尾。始终保留最后一行（可能为空）。
void split_lines(std::string_view source, std::vector<std::string_view>& out) {
    out.clear();
    std::size_t start = 0;
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\n' || source[i] == '\r') {
            out.push_back(source.substr(start, i - start));
            if (source[i] == '\r' && i + 1 < source.size() && source[i + 1] == '\n') {
                ++i;
            }
            start = i + 1;
        }
    }
    out.push_back(source.substr(start));
}

// 行尾的 `|` 是否被反斜杠转义（前面有奇数个连续的 '\'）。
bool ends_with_escaped_pipe(std::string_view line) noexcept {
    if (line.empty() || line.back() != '|') {
        return false;
    }
    std::size_t backslashes = 0;
    std::size_t i = line.size() - 1;
    while (i > 0 && line[i - 1] == '\\') {
        ++backslashes;
        --i;
    }
    return (backslashes % 2) == 1;
}

// 行内是否存在"未转义且不在代码段内"的 `|`（用于判断某行是否属于表格）。
bool has_column_pipe(std::string_view line) noexcept {
    bool in_code = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '`') {
            in_code = !in_code;
            continue;
        }
        if (in_code) {
            continue;
        }
        if (ch == '\\' && i + 1 < line.size()) {
            ++i;
            continue;
        }
        if (ch == '|') {
            return true;
        }
    }
    return false;
}

// 切出行内的单元格：遇未转义且不在代码段内的 `|` 分列；
// `\|` → `|`、`\\` → `\`，其余反斜杠原样保留。
void split_cells(std::string_view text, std::vector<std::string>& out) {
    out.clear();
    std::string cell;
    bool in_code = false;
    std::size_t i = 0;
    while (i < text.size()) {
        const char ch = text[i];
        if (ch == '`') {
            in_code = !in_code;
            cell.push_back(ch);
            ++i;
            continue;
        }
        if (!in_code && ch == '\\' && i + 1 < text.size()) {
            const char next = text[i + 1];
            if (next == '|') {
                cell.push_back('|');
                i += 2;
                continue;
            }
            if (next == '\\') {
                cell.push_back('\\');
                i += 2;
                continue;
            }
            cell.push_back(ch);
            ++i;
            continue;
        }
        if (!in_code && ch == '|') {
            out.push_back(trim_cell(cell));
            cell.clear();
            ++i;
            continue;
        }
        cell.push_back(ch);
        ++i;
    }
    out.push_back(trim_cell(cell));
}

// 解析一整行为单元格：去掉可选的行首/行尾 `|` 后再切分。
void parse_row(std::string_view raw_line, std::vector<std::string>& out) {
    std::string_view line = trim(raw_line);
    if (!line.empty() && line.front() == '|') {
        line.remove_prefix(1);
    }
    if (!line.empty() && line.back() == '|' && !ends_with_escaped_pipe(line)) {
        line.remove_suffix(1);
    }
    split_cells(line, out);
}

// 分隔行单元格：`-+` 或 `:-+` / `-+:` / `:-+:`
bool parse_alignment_cell(std::string_view cell, TableAlignment::Kind& out) noexcept {
    std::string_view s = trim(cell);
    if (s.empty()) {
        return false;
    }
    const bool left = s.front() == ':';
    const bool right = s.back() == ':';
    if (left) {
        s.remove_prefix(1);
    }
    if (right && !s.empty()) {
        s.remove_suffix(1);
    }
    if (s.empty()) {
        return false;
    }
    for (const char ch : s) {
        if (ch != '-') {
            return false;
        }
    }
    if (left && right) {
        out = TableAlignment::Kind::Center;
    } else if (left) {
        out = TableAlignment::Kind::Left;
    } else if (right) {
        out = TableAlignment::Kind::Right;
    } else {
        out = TableAlignment::Kind::None;
    }
    return true;
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
                continue;  // \r\n 只算一个行尾，交给 '\n' 处理
            }
            ++line;
            column = 1;
        } else {
            ++column;
        }
    }
}

std::size_t offset_of(std::string_view source, std::string_view line) noexcept {
    return static_cast<std::size_t>(line.data() - source.data());
}

}  // namespace

Result<document::TableModel> parse_markdown_table(std::string_view source,
                                                  const MarkdownTableOptions& options,
                                                  ParseDiagnostic* out_diagnostic,
                                                  TableAlignment* out_alignment) {
    auto fail = [&](ErrorCode code, std::string message,
                    std::size_t offset) -> Result<document::TableModel> {
        if (out_diagnostic != nullptr) {
            out_diagnostic->code = code;
            out_diagnostic->message = std::move(message);
            out_diagnostic->offset = offset;
            position_of(source, offset, out_diagnostic->line, out_diagnostic->column);
        }
        return Unexpected<ErrorCode>(code);
    };

    std::vector<std::string_view> lines;
    split_lines(source, lines);

    std::size_t idx = 0;
    while (idx < lines.size() && trim(lines[idx]).empty()) {
        ++idx;
    }
    if (idx >= lines.size()) {
        return fail(ErrorCode::ParseError, "空输入：没有表格", 0);
    }
    const std::string_view header_line = lines[idx];
    if (!has_column_pipe(header_line)) {
        return fail(ErrorCode::ParseError, "表头缺少 `|` 列分隔", offset_of(source, header_line));
    }

    std::vector<std::string> header;
    parse_row(header_line, header);
    const std::size_t columns = header.size();

    // ── 分隔行 ─────────────────────────────────────────────────────────
    bool separator_valid = false;
    std::vector<TableAlignment::Kind> alignment;
    if (idx + 1 < lines.size()) {
        std::vector<std::string> delimiter_cells;
        parse_row(lines[idx + 1], delimiter_cells);
        std::vector<TableAlignment::Kind> kinds;
        kinds.reserve(delimiter_cells.size());
        bool all_specifiers = !delimiter_cells.empty();
        for (const std::string& cell : delimiter_cells) {
            TableAlignment::Kind kind = TableAlignment::Kind::None;
            if (!parse_alignment_cell(cell, kind)) {
                all_specifiers = false;
                break;
            }
            kinds.push_back(kind);
        }
        if (all_specifiers && kinds.size() == columns) {
            separator_valid = true;
            alignment = std::move(kinds);
        }
    }
    if (options.require_separator && !separator_valid) {
        const std::size_t offset = offset_of(source, lines[idx]);
        return fail(ErrorCode::ParseError, "缺少与表头列数一致的分隔行", offset);
    }

    document::TableModel model;
    model.set_header(std::move(header));

    // ── 数据行 ─────────────────────────────────────────────────────────
    std::size_t data_begin = idx + 1 + (separator_valid ? 1u : 0u);
    std::size_t row_count = 0;
    for (std::size_t i = data_begin; i < lines.size(); ++i) {
        const std::string_view line = lines[i];
        if (trim(line).empty()) {
            break;  // 空行结束表格
        }
        if (!has_column_pipe(line)) {
            break;  // 非表格行结束表格
        }
        if (row_count >= options.max_rows) {
            return fail(ErrorCode::OutOfRange, "数据行超过 max_rows", offset_of(source, line));
        }
        std::vector<std::string> cells;
        parse_row(line, cells);
        cells.resize(columns);  // 多出的列裁剪、缺失的列补空串
        model.add_row(std::move(cells));
        ++row_count;
    }

    if (out_alignment != nullptr) {
        out_alignment->columns = std::move(alignment);
        out_alignment->columns.resize(columns, TableAlignment::Kind::None);
    }
    return model;
}

}  // namespace termchart::dsl
