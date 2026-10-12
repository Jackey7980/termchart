// table_markdown.hpp —— GitHub-Flavored Markdown 管道表格（L17）
//
// 支持范围（**明确列出，避免"以为支持"**）：
//   ✅ 表头行 + 分隔行（`---` / `:---` / `---:` / `:---:`）+ 若干数据行
//   ✅ 行首尾的 `|` 可省；单元格前后空白被裁剪
//   ✅ 单元格内 `\|` 还原为字面 `|`；反引号代码段内的 `|` 不作列分隔
//   ✅ 对齐信息（None/Left/Center/Right）经 TableAlignment 出参返回
//   ⛔ 认出但忽略：除 `\|` 与 `\\` 外的反斜杠转义按原文保留，不做解释；
//      未闭合的反引号按普通文本处理（不报错）
//   ⛔ 本切片不支持：HTML `<table>`、单元格内换行、colspan/rowspan、嵌套表格
//
// 列数策略：**以表头列数为准**。数据行多出的列被裁剪、缺失的列补空串；
// 分隔行必须与表头列数一致，否则按解析错误处理（GFM 要求相等）。
// 表格在首个空行或首个不含 `|` 的行处结束。
//
// 诊断：解析失败返回错误码，可选地把**首个**诊断写到 out_diagnostic。
#ifndef TERMCHART_DSL_TABLE_MARKDOWN_HPP
#define TERMCHART_DSL_TABLE_MARKDOWN_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/core/expected.hpp"
#include "termchart/document/table_model.hpp"
#include "termchart/dsl/parser_base.hpp"
#include "termchart/export.hpp"

namespace termchart::dsl {

// 列对齐：与 GFM 分隔行语义一一对应（None = 默认左对齐）。
struct TableAlignment {
    enum class Kind : std::uint8_t { None = 0, Left, Center, Right };
    std::vector<Kind> columns;
};

struct MarkdownTableOptions {
    bool require_separator = true;   // 缺分隔行（|---|---|）时视为错误（GFM 要求有）
    std::size_t max_rows = 10000;    // 数据行上限（不含表头与分隔行）
};

// 解析一张 GFM 管道表格：
//   * 成功返回 document::TableModel（表头 + 数据行，行已规范到表头列数）；
//   * 失败返回 ErrorCode::ParseError / OutOfRange，可选写出首个诊断；
//   * out_alignment 非空时写入每列对齐方式（长度 = 表头列数；无分隔行时全 None）。
[[nodiscard]] TERMCHART_API Result<document::TableModel> parse_markdown_table(
    std::string_view source, const MarkdownTableOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr, TableAlignment* out_alignment = nullptr);

}  // namespace termchart::dsl

#endif  // TERMCHART_DSL_TABLE_MARKDOWN_HPP
