// dataframe.cpp —— 容错 CSV 读取实现（"法医级"宽容读取器）
//
// 字段切分/引号/嗅探逻辑与 io/csv 同源（小段复制进本文件匿名命名空间，**不改动
// io/csv**），差别仅在产出：每个字段变成一个 Cell，文本/脏值不再报错而是标记，
// 行列不齐通过 Ragged 空位表达。详见 data/dataframe.hpp 头注释。
#include "termchart/data/dataframe.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "src/data/cell_flags_internal.hpp"

namespace termchart::data {
namespace {

constexpr std::string_view kBom = "\xEF\xBB\xBF";

bool starts_with_bom(std::string_view s) noexcept {
    return s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF &&
           static_cast<unsigned char>(s[1]) == 0xBB &&
           static_cast<unsigned char>(s[2]) == 0xBF;
}

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
    std::size_t best = 0;
    for (int c = 1; c < 4; ++c) {
        if (counts[c] > counts[best]) best = static_cast<std::size_t>(c);
    }
    return counts[best] > 0 ? kCandidates[best] : ',';
}

void trim_inplace(std::string_view& s) noexcept {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
}

// 解析一行字段（不含行尾）。视图在 source 存活期内有效；含 "" 转义的字段退化为 owned。
bool parse_row(std::string_view source, std::size_t& pos, char delim,
               std::vector<std::string_view>& fields, std::string& owned_escape) noexcept {
    fields.clear();
    while (true) {
        std::string_view field;
        if (pos < source.size() && source[pos] == '"') {
            ++pos;
            owned_escape.clear();
            bool escaped = false;
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
                    ++pos;
                    closed = true;
                    break;
                }
                ++pos;
            }
            if (!closed) return false;  // 引号未闭合：宽容读取器仍返回已解析部分，不报错
            if (escaped) {
                owned_escape.append(source.substr(plain_start, pos - 1 - plain_start));
                field = std::string_view(owned_escape);
            } else {
                field = source.substr(plain_start, pos - 1 - plain_start);
            }
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
            continue;
        }
        break;
    }
    if (pos < source.size() && source[pos] == '\r') ++pos;
    if (pos < source.size() && source[pos] == '\n') ++pos;
    return true;
}

// 映射函数已移到 cell_flags_internal.hpp：JSON 轨必须用同一份，否则两边口径漂移。

// 字段 → Cell：清洗成数值并记 flag；文本/缺失保留原文/标记，绝不报错。
Cell make_cell(std::string_view field) {
    Cell cell;
    std::string_view trimmed = field;
    trim_inplace(trimmed);
    cell.raw.assign(trimmed);
    const ParsedScalar p = parse_scalar(trimmed);
    cell.unit = p.unit;
    const CellFlag mapped = detail::map_scalar_flags(p.flags);
    switch (p.status) {
        case ScalarStatus::Numeric:
            cell.type = CellType::Numeric;
            cell.num = p.value;
            cell.flags = mapped;
            break;
        case ScalarStatus::Empty:
            cell.type = CellType::Empty;
            cell.flags = mapped | CellFlag::Missing;
            break;
        case ScalarStatus::Invalid:
        default:
            // 认不出：保留原文给上层做分类维度，绝不丢弃。
            cell.type = CellType::Text;
            cell.flags = mapped | CellFlag::Unparsed;
            break;
    }
    return cell;
}

Cell ragged_empty() noexcept {
    Cell cell;
    cell.type = CellType::Empty;
    cell.flags = CellFlag::Missing | CellFlag::Ragged;
    return cell;
}

}  // namespace

DataFrame parse_dataframe(std::string_view source, const DataFrameOptions& options) {
    DataFrame frame;
    std::size_t pos = 0;
    if (starts_with_bom(source)) pos = kBom.size();

    const char delim =
        options.delimiter == 0 ? sniff_delimiter(source, pos) : options.delimiter;

    std::vector<std::string_view> fields;
    std::string owned_escape;
    [[maybe_unused]] std::size_t line_no = 1;
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
        if (!parse_row(source, pos, delim, fields, owned_escape)) {
            // 引号未闭合：宽容读取器忽略该行剩余、继续，不报错。
            while (pos < source.size() && source[pos] != '\n') ++pos;
            if (pos < source.size()) ++pos;
            ++line_no;
            continue;
        }
        ++line_no;

        if (!header_done && options.has_header) {
            frame.headers.reserve(fields.size());
            for (const std::string_view f : fields) {
                std::string_view t = f;
                trim_inplace(t);
                frame.headers.emplace_back(t);
            }
            header_done = true;
            continue;
        }

        const std::size_t r = frame.row_count;
        // 确保列数足够容纳本行字段。
        if (frame.columns.size() < fields.size()) {
            frame.columns.resize(fields.size());
        }
        for (std::size_t c = 0; c < fields.size(); ++c) {
            // 补齐本列此前缺失的行（本行之前该列不存在 → 补 Ragged 空位）。
            while (frame.columns[c].size() < r) {
                frame.columns[c].push_back(ragged_empty());
            }
            frame.columns[c].push_back(make_cell(fields[c]));
        }
        // 本行字段不足：尾部已有列补 Ragged 空位。
        for (std::size_t c = fields.size(); c < frame.columns.size(); ++c) {
            while (frame.columns[c].size() < r) {
                frame.columns[c].push_back(ragged_empty());
            }
            frame.columns[c].push_back(ragged_empty());
        }
        ++frame.row_count;
    }

    if (frame.headers.empty() && !options.has_header) {
        for (std::size_t c = 0; c < frame.columns.size(); ++c) {
            frame.headers.push_back("col" + std::to_string(c));
        }
    }
    // 保证每列长度 == row_count（表头前若有数据行的极端情形下的兜底）。
    for (std::vector<Cell>& col : frame.columns) {
        while (col.size() < frame.row_count) col.push_back(ragged_empty());
    }
    return frame;
}

}  // namespace termchart::data
