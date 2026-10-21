#ifndef TERMCHART_DATA_DATAFRAME_HPP
#define TERMCHART_DATA_DATAFRAME_HPP

// data/dataframe.hpp —— 容错 CSV 读取（"法医级"宽容读取器，L4）
//
// 与 io/csv 的分工（**刻意两份，不是遗漏**）：
//   * io/csv：严格数值入口——列主序 vector<vector<double>>，遇到文本列/脏值/行列不齐
//     直接 fail-fast（ParseError）。适合"已知干净的数值表"。
//   * 本文件：宽容读取器——每个字段是一个 Cell（原文 + 解析值 + 类型 + 质量标记），
//     文本列保留原文（分类维度），脏值清洗成数值并记 flag，缺列补 Empty(Ragged)，
//     **整张表绝不报错**，任何一行都继续读。适合 AutoViz 的"地狱级脏表"入口。
//
// 底层哲学差异：io/csv = Fail-fast（干净数据规整器）；data/dataframe = Fail-safe
// （垃圾数据标记并继续）。两者并存，互不污染。

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/data/scalar_parse.hpp"

namespace termchart::data {

// 单元格类型：Empty=缺失，Numeric=解析成数，Text=原文保留（分类/无法解析），
// Invalid=解析异常（理论上 Text 已覆盖，保留以备扩展）。
enum class CellType : std::uint8_t { Empty, Numeric, Text, Invalid };

// 单元格质量标记位（由 scalar_parse 的 ScalarFlag 派生 + 表级特征）。
enum class CellFlag : std::uint8_t {
    None = 0,
    HasComma = 1 << 0,           // 千分位
    AccountingNegative = 1 << 1, // 会计括号负
    Percent = 1 << 2,            // 百分号
    FullWidth = 1 << 3,          // 全角
    Currency = 1 << 4,           // 货币/单位
    Missing = 1 << 5,            // 缺失（空/NA）
    Ragged = 1 << 6,             // 行缺列补的空位
    Unparsed = 1 << 7,           // 原文保留（文本/解析失败）
};

constexpr CellFlag operator|(CellFlag a, CellFlag b) noexcept {
    return static_cast<CellFlag>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
constexpr CellFlag operator&(CellFlag a, CellFlag b) noexcept {
    return static_cast<CellFlag>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}
constexpr bool has_cell_flag(CellFlag f, CellFlag bit) noexcept {
    return (static_cast<std::uint8_t>(f) & static_cast<std::uint8_t>(bit)) != 0;
}

// 一个单元格：原文 + 解析值 + 类型 + 质量标记 + 单位。
struct Cell {
    std::string raw;                          // 原始字段（去引号、去前后空白）
    double num = std::nan("");                // Numeric 时为解析值
    CellType type = CellType::Empty;
    CellFlag flags = CellFlag::None;
    std::string unit;                         // 货币/单位后缀（如 "元"），正常为空

    [[nodiscard]] bool is_numeric() const noexcept { return type == CellType::Numeric; }
    [[nodiscard]] bool is_text() const noexcept { return type == CellType::Text; }
    [[nodiscard]] bool is_missing() const noexcept {
        return type == CellType::Empty || has_cell_flag(flags, CellFlag::Missing);
    }
};

// 列主序表：columns[c] 为第 c 列（长度 = 行数，缺列处为 Empty(Ragged)）。
// 不再强制矩形——行长度不一通过 Ragged 空位表达，绝不报错。
struct DataFrame {
    std::vector<std::string> headers;            // 表头（has_header=false 时为 col0..）
    std::vector<std::vector<Cell>> columns;      // 列主序
    std::size_t row_count = 0;
};

struct DataFrameOptions {
    char delimiter = 0;    // 0 = 自动嗅探（, ; \t | 中引号外最多者）
    bool has_header = true;
    char comment = '#';    // '#' 前缀行跳过；0 = 不启用
};

// 容错解析 CSV 文本为 DataFrame。任何输入都返回（可能满是 Empty/Text 的）表，不报错。
// 空输入返回空表（0 行 0 列），合法。
[[nodiscard]] DataFrame parse_dataframe(std::string_view source,
                                        const DataFrameOptions& options = {});

}  // namespace termchart::data

#endif  // TERMCHART_DATA_DATAFRAME_HPP
