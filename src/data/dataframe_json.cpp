// dataframe_json.cpp —— JSON/JSONL → DataFrame 实现
//
// 装配策略：行流式消费（不缓存整表 DOM），键序用"首次出现序 + 追加"维护，
// 新列对既有行补 Empty、新行对既有列补 Empty——两侧都不错位。
#include "termchart/data/dataframe_json.hpp"

#include <charconv>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/data/cell_flags_internal.hpp"
#include "termchart/data/scalar_parse.hpp"
#include "termchart/io/json.hpp"

namespace termchart::data {
namespace {

// double → 最短往返表示（std::to_chars 默认格式，避免 to_string 的 6 位截断）。
std::string number_text(double value) {
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, result.ptr);
}

// JSON 标量 → Cell。字符串走 scalar_parse（"$1,000"/"15%"/全角在 JSON 字符串
// 里同样脏，清洗口径与 CSV 轨一致）；数字直接用 DOM 的 double（JSON 已定型，
// 无需再猜）；bool 记 Text 原文；null → Empty。
Cell cell_from_scalar(const io::JsonValue& value) {
    Cell cell;
    switch (value.type()) {
    case io::JsonValue::Type::Null:
        cell.type = CellType::Empty;
        return cell;
    case io::JsonValue::Type::Bool:
        cell.raw = value.as_bool() ? "true" : "false";
        cell.type = CellType::Text;
        return cell;
    case io::JsonValue::Type::Number: {
        const double number = value.as_number();
        cell.raw = number_text(number);
        cell.num = number;
        cell.type = CellType::Numeric;
        return cell;
    }
    case io::JsonValue::Type::String: {
        const std::string_view text = value.as_string();
        const ParsedScalar parsed = parse_scalar(text);
        cell.raw = std::string(text);
        // 与 CSV 轨共用同一份 ScalarFlag → CellFlag 映射。这一步曾经漏掉，导致
        // JSON 里的 "15%" 只剩 0.15 而丢掉 Percent 标记，下游列剖析看不出量纲差异
        // （CSV 轨有标记、JSON 轨没有，同一份脏数据两条入口两种结论）。
        cell.flags = detail::map_scalar_flags(parsed.flags);
        switch (parsed.status) {
        case ScalarStatus::Numeric:
            cell.num = parsed.value;
            cell.type = CellType::Numeric;
            break;
        case ScalarStatus::Empty:
            cell.type = CellType::Empty;
            cell.flags = cell.flags | CellFlag::Missing;
            break;
        case ScalarStatus::Invalid:
            cell.type = CellType::Text;
            cell.flags = cell.flags | CellFlag::Unparsed;
            break;
        }
        cell.unit = parsed.unit;
        return cell;
    }
    case io::JsonValue::Type::Array:
    case io::JsonValue::Type::Object:
        break;  // 容器由 flatten 处理，不会到这里
    }
    cell.type = CellType::Empty;
    return cell;
}

// 递归展平：对象按键、数组按索引，点路径拼接。空容器不产列（没有可放的标量）。
void flatten(const io::JsonValue& value, const std::string& prefix,
             std::vector<std::pair<std::string, Cell>>& out) {
    if (value.is_object()) {
        for (const std::string& key : value.keys()) {
            const std::string path = prefix.empty() ? key : prefix + "." + key;
            flatten(value.at(key), path, out);
        }
        return;
    }
    if (value.is_array()) {
        for (std::size_t i = 0; i < value.size(); ++i) {
            flatten(value[i], prefix.empty() ? std::to_string(i) : prefix + "." + std::to_string(i),
                    out);
        }
        return;
    }
    if (prefix.empty()) {
        return;  // 顶层标量：无键可放，由调用方按非记录行处理
    }
    out.emplace_back(prefix, cell_from_scalar(value));
}

// 行装配器：列序首次出现序，双向补空不错位。
class FrameBuilder {
public:
    void add_record(const io::JsonValue& object) {
        std::vector<std::pair<std::string, Cell>> cells;
        flatten(object, {}, cells);
        // 顺序有讲究：先 ensure_column（新列为**既有行**补 Empty），再给所有列
        // 统一追加当前行槽位，最后赋值——反序会让新列少一个槽位、back() 错落到上一行。
        for (const auto& [name, cell] : cells) {
            (void)ensure_column(name);  // 返回新增列索引，这里仅需要确保列存在
        }
        for (auto& column : columns_) {
            column.emplace_back();
        }
        for (auto& [name, cell] : cells) {
            columns_[index_[name]].back() = std::move(cell);
        }
        ++rows_;
    }

    [[nodiscard]] JsonFrameResult finish() const {
        JsonFrameResult result;
        result.frame.headers = order_;
        result.frame.columns = columns_;
        result.frame.row_count = rows_;
        result.lines_used = rows_;
        result.lines_skipped = lines_skipped_;
        result.lines_zipped = lines_zipped_;
        return result;
    }

    // 位置式数组（["X004", 1740787200, "(123.45)", ...]）按列序 zip 成一行。
    // 仅仅"宽度恰好等于当前列数"才成立——列序是首现序，宽度相同说明这条数组与
    // 已建立的 schema 同宽；宽度不同一律放弃：把 5 个元素强行摊到 14 列上，
    // "X004" 会落进 ts 列，得到一行看着合法、实际全错位的假记录。
    // 容器元素（对象/数组）塞不进标量格，按缺失处理——不替调用方序列化成字符串。
    bool add_positional(const io::JsonValue& array) {
        if (order_.empty() || array.size() != order_.size()) {
            return false;
        }
        for (auto& column : columns_) {
            column.emplace_back();
        }
        for (std::size_t c = 0; c < columns_.size(); ++c) {
            columns_[c].back() = cell_from_scalar(array[c]);
        }
        ++rows_;
        ++lines_zipped_;
        return true;
    }

    void count_line(bool used) {
        ++lines_total_;
        if (!used) {
            ++lines_skipped_;
        }
    }

    [[nodiscard]] std::size_t lines_total() const noexcept { return lines_total_; }

private:
    [[nodiscard]] std::size_t ensure_column(const std::string& name) {
        const auto it = index_.find(name);
        if (it != index_.end()) {
            return it->second;
        }
        const std::size_t index = order_.size();
        index_.emplace(name, index);
        order_.push_back(name);
        columns_.emplace_back();
        columns_.back().resize(rows_);  // 新列：为既有行补 Empty
        return index;
    }

    std::vector<std::string> order_;
    std::vector<std::vector<Cell>> columns_;
    std::unordered_map<std::string, std::size_t> index_;
    std::size_t rows_ = 0;
    std::size_t lines_total_ = 0;
    std::size_t lines_skipped_ = 0;
    std::size_t lines_zipped_ = 0;
};

// 单行 JSON 值 → 0..N 条记录。返回记录数；非记录形态返回 0。
std::size_t add_line(FrameBuilder& builder, const io::JsonValue& value) {
    if (value.is_object()) {
        builder.add_record(value);
        return 1;
    }
    if (value.is_array()) {
        std::size_t added = 0;
        bool all_objects = value.size() > 0;  // 空数组不当记录集（避免产出 0 行假记录）
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (!value[i].is_object()) {
                all_objects = false;
                break;
            }
        }
        if (all_objects) {
            for (std::size_t i = 0; i < value.size(); ++i) {
                builder.add_record(value[i]);
                ++added;
            }
            return added;
        }
        // 标量数组：宽度对上就按列序救回来（见 add_positional 的取舍）。
        if (builder.add_positional(value)) {
            return 1;
        }
        return 0;
    }
    return 0;
}

}  // namespace

Result<JsonFrameResult> dataframe_from_jsonl(std::string_view source,
                                             const JsonFrameOptions& options) {
    FrameBuilder builder;
    std::size_t line_start = 0;
    std::size_t consumed_lines = 0;
    while (line_start <= source.size()) {
        if (options.max_lines != 0 && consumed_lines >= options.max_lines) {
            break;
        }
        std::size_t line_end = source.find('\n', line_start);
        const bool last = line_end == std::string_view::npos;
        if (last) {
            line_end = source.size();
        }
        std::string_view line = source.substr(line_start, line_end - line_start);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        line_start = last ? source.size() + 1 : line_end + 1;

        // 空白行不计入总数（JSONL 允许空行分隔）。
        bool blank = true;
        for (const char ch : line) {
            if (ch != ' ' && ch != '\t' && ch != '\r') {
                blank = false;
                break;
            }
        }
        if (blank) {
            if (last) {
                break;
            }
            continue;
        }
        ++consumed_lines;

        const Result<io::JsonValue> parsed = io::parse_json(line);
        if (!parsed.has_value()) {
            if (!options.skip_invalid_lines) {
                return Unexpected<ErrorCode>(parsed.error());
            }
            builder.count_line(false);
            if (last) {
                break;
            }
            continue;
        }
        const std::size_t added = add_line(builder, *parsed);
        builder.count_line(added > 0);
        if (last) {
            break;
        }
    }

    JsonFrameResult result = builder.finish();
    result.lines_total = builder.lines_total();
    return result;
}

Result<JsonFrameResult> dataframe_from_json(std::string_view source,
                                            const JsonFrameOptions& options) {
    const Result<io::JsonValue> parsed = io::parse_json(source);
    if (!parsed.has_value()) {
        return Unexpected<ErrorCode>(parsed.error());
    }
    const io::JsonValue& value = *parsed;

    FrameBuilder builder;
    if (value.is_array()) {
        bool all_objects = value.size() > 0;
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (!value[i].is_object()) {
                all_objects = false;
                break;
            }
        }
        if (!all_objects) {
            return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
        }
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (options.max_lines != 0 && i >= options.max_lines) {
                break;
            }
            builder.add_record(value[i]);
        }
    } else if (value.is_object()) {
        builder.add_record(value);
    } else {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }

    JsonFrameResult result = builder.finish();
    result.lines_total = result.lines_used;
    return result;
}

}  // namespace termchart::data
