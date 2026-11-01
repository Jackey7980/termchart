// csv_sniff.hpp —— CSV 解析与列类型推断（L17）
//
// 支持范围（**明确列出，避免"以为支持"**）：
//   ✅ 自定义单字符 delimiter；表头可选（has_header=false 时列名为 col1..colN）
//   ✅ 引号字段 "..."：内含分隔符与换行；"" 表示一个引号；未闭合 → ParseError
//   ✅ 开头 UTF-8 BOM 跳过（首个列名不带 BOM）
//   ✅ 列类型推断：Boolean → Integer → Real → Date → Text；整列空 → Empty
//   ⛔ 认出但忽略：闭合引号后到分隔符/行尾的剩余字符按字面并入字段（宽松，不报错）；
//      引号字段内的 CR/LF 原样保留
//   ⛔ 本切片不支持：多字符分隔符、注释行、`sep=` 指令、反斜杠转义
//      （CSV 只认引号翻倍）、delimiter 之外的方言自动识别
//
// 行/列策略：完全空白的行被跳过；推断时缺失的列按"空"计。
// 类型判定顺序（先命中先定；列内所有非空样本须全部满足）：
//   Empty（无非空样本）→ Boolean → Integer → Real → Date → Text。
//   Boolean：true/false/yes/no/0/1（大小写不敏感）；
//   Integer：可选正负号 + 纯数字；Real：十进制 / 科学计数法；
//   Date：YYYY-MM-DD，可选 (空格|T) HH:MM(:SS)。
// 样本前先裁剪首尾空白；sniff_rows==0 表示用全部数据行，否则只用前 N 个数据行。
//
// 诊断：解析失败返回错误码，可选地把**首个**诊断写到 out_diagnostic；
// 未闭合引号的位置指向**起始引号**。
#ifndef TERMCHART_DSL_CSV_SNIFF_HPP
#define TERMCHART_DSL_CSV_SNIFF_HPP

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/core/expected.hpp"
#include "termchart/document/table_model.hpp"
#include "termchart/dsl/parser_base.hpp"
#include "termchart/export.hpp"

namespace termchart::dsl {

enum class ColumnType : std::uint8_t {
    Empty = 0,
    Boolean,
    Integer,
    Real,
    Date,
    Text,
};

struct ColumnProfile {
    std::string name;
    ColumnType type = ColumnType::Empty;
    std::size_t filled = 0;   // 非空样本数
    std::size_t empty = 0;    // 空样本数
};

struct CsvOptions {
    char delimiter = ',';
    bool has_header = true;
    std::size_t sniff_rows = 100;    // 只用前 N 行做类型推断；0 = 全部行
    std::size_t max_rows = 10000;    // 数据行上限（不含表头）
};

// 解析 CSV 为 document::TableModel：has_header 时首行作表头，其余为数据行。
// 失败返回 ErrorCode::ParseError / OutOfRange / InvalidArgument。
//
// ⚠️ 内存口径：本函数**整表物化**（输入 + TableModel 同时在内存里），
// 适合"整表都要"的场景；只想抽样看类型请用 sniff_columns，
// 数据大到放不下时请用 parse_csv_stream（内存 O(一行)）。
[[nodiscard]] TERMCHART_API Result<document::TableModel> parse_csv(
    std::string_view source, const CsvOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

// 逐列推断类型，返回每列的 ColumnProfile（name/type/filled/empty）。
// 列数：has_header 时取表头列数，否则取**样本行**的最大列数。
//
// 内存口径：只读前 sniff_rows 个数据行做推断（sniff_rows==0 才读全部），
// 不会为了推断 100 行把整个文件读进来。
[[nodiscard]] TERMCHART_API Result<std::vector<ColumnProfile>> sniff_columns(
    std::string_view source, const CsvOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

// 流式 CSV 的行接收器。
//
// 语义：每读到一行回调一次；fields 是该行按 CSV 规则解析出的字段。
// has_header=true 时**第一次回调拿到的是表头**（字段即列名），其后是数据行。
// 返回 false 表示"够了，别再读了"——解析随即正常结束（不是错误）。
class TERMCHART_API CsvRowSink {
public:
    virtual ~CsvRowSink() = default;
    virtual bool on_row(std::vector<std::string>& fields) = 0;
};

// 流式解析 CSV：逐行喂给 sink，内存占用 **O(一行)**，与文件大小无关。
//
// 适用场景：GB 级 CSV 直接喂给图表管线，不落中间表。
// 失败返回 ParseError（引号未闭合）/ OutOfRange（超过 max_rows）/ InvalidArgument（非法分隔符）。
// 行号列号在流式下是增量统计的，与整表解析口径一致。
[[nodiscard]] TERMCHART_API Status parse_csv_stream(
    std::istream& input, const CsvOptions& options, CsvRowSink& sink,
    ParseDiagnostic* out_diagnostic = nullptr);

}  // namespace termchart::dsl

#endif  // TERMCHART_DSL_CSV_SNIFF_HPP
