// io/csv.hpp —— CSV 数值入口（零拷贝切分，L2）
//
// 与 dsl/csv_sniff 的分工（**刻意两份，不是遗漏**）：
//   * 本文件：数值数据进图表的快路径——string_view 切分 + from_chars 直转
//     double，列主序 vector<vector<double>>，空字段/NA 落 NaN；
//   * dsl/csv_sniff：方言分析与表格文档（列类型推断 → document::TableModel），
//     它依赖 document（L8），而 io=2 不能 include L8，**结构上无法下沉合并**；
//     未来若合并，需先把它的 TableModel 组装层拆到 L8+，记入 backlog。
//
// 支持范围（明确列出，避免"以为支持"）：
//   ✅ 单字符 delimiter，0 = 自动嗅探（, ; \t | 中引号外出现最多者）；
//   ✅ 表头可选；UTF-8 BOM 跳过；注释行（comment 前缀，允许前导空白）；
//   ✅ 引号字段 "..."：内含分隔符与换行；"" 表示一个字面引号；
//      无转义的字段走零拷贝 view 快路径，含 "" 的字段退化为一次分配；
//   ✅ 行尾 LF / CRLF / CR；
//   ✅ 缺值口径：空字段与 "NaN"/"nan"/"NA"（大小写不敏感）→ quiet NaN。
//   ⛔ 非数字字段（如文本列）→ ParseError（带行列诊断）：这是数值入口，
//      要容忍文本请走 dsl/csv_sniff 的 TableModel；
//   ⛔ 数字前导 + 号（from_chars 口径，与 io/json 一致）；
//   ⛔ 反斜杠转义、多字符分隔符、`sep=` 指令。
#ifndef TERMCHART_IO_CSV_HPP
#define TERMCHART_IO_CSV_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"
#include "termchart/io/diagnostic.hpp"

namespace termchart::io {

struct CsvOptions {
    char delimiter = 0;    // 0 = 自动嗅探
    bool has_header = true;
    char comment = '#';    // '#' 前缀行跳过；0 = 不启用
};

struct CsvTable {
    std::vector<std::string> headers;            // has_header=false 时为 col0..colN-1
    std::vector<std::vector<double>> columns;    // 列主序
    std::size_t row_count = 0;
};

// 解析 CSV 文本为数值表。失败返回 ErrorCode::ParseError / OutOfRange /
// InvalidArgument，可选把首个诊断（行列 + 偏移）写到 out_diagnostic。
// 空输入返回空表（0 行 0 列），合法。
[[nodiscard]] TERMCHART_API Result<CsvTable> parse_csv(
    std::string_view source, const CsvOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

}  // namespace termchart::io

#endif  // TERMCHART_IO_CSV_HPP
