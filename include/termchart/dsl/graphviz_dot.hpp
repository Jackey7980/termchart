// graphviz_dot.hpp —— Graphviz DOT 语法（L17）
//
// 支持范围（**明确列出，避免"以为支持"**）：
//   ✅ 头部 `digraph [名字] {` / `graph [名字] {`（`strict` 前缀忽略）；`}` 收尾（缺失 → ParseError）
//   ✅ 方向：`rankdir=LR|TB|BT|RL` / `rankdir="LR"`，以及属性行 `graph [rankdir=LR]`
//      （值统一转大写写进 AstGraph::direction；DOT 默认 TB，故未写时留空，由渲染侧取默认）
//   ✅ 节点：`a;`、`a [label="文字", shape=box]`、多条属性列表 `a [k=v][k2="v2"]`
//      （`label`→AstNode::label；`shape` 认 box/ellipse/diamond/hexagon/cylinder/round，
//        其余 shape 原样落进 AstNode::attributes，不静默丢弃）
//   ✅ 边：`a -> b`（有向）、`a -- b`（无向）、链 `a -> b -> c`、`a -> b [label="x"]`
//      （边属性 `style=dashed|dotted`→line_style="dashed"，`style=bold`→"thick"，
//        `dir=both`→ArrowKind::Both）
//   ✅ 并列节点集：`a -> {b c}`、`{a b} -> c`、`a -> {b c} -> d`（按笛卡尔积展开为多条边）
//   ⛔ 认出但**忽略**（不产生节点/边）：
//      `strict`；`graph/node/edge [attr]` 默认属性行（`graph` 行里的 rankdir 仍会被取用）；
//      `subgraph 名称 { … }` 块（块头与 `}` 被吞掉，块内语句按顶层继续解析，不建分组属性）；
//      `id = value` 形式的图属性赋值（仅 `rankdir` 有效，其余丢弃）
//   ⛔ **不支持**（不做或降级，且不假装支持）：
//      HTML 风格标签 `<…>`（会被当作标点拆散，含它的语句报 ParseError）；
//      端口 `a:port`（端口被跳过、port 名丢失）；一个文件里多张图（只解析第一张）；
//      `->` 之外带属性语义的复杂边（如 `dir=none` 之外的方向微调）
//
// 为什么这样切：DOT 的属性列表允许任意 k=v 且值可含空格/标点/中文，按 token 拼接极易出错，
// 因此属性一律用**原文切片**读取（`[` 到配对 `]` 之间），再在切片上按逗号/等号切分——
// 于是 `label="a -> b"` 这类"引号里长得像箭头"的内容天然不会被误解。
//
// 诊断：失败返回解析器错误码（ParseError 语法错 / OutOfRange 超 max_nodes），
//       可选地把首个诊断写到 out_diagnostic；任何路径都**不抛异常**。
#ifndef TERMCHART_DSL_GRAPHVIZ_DOT_HPP
#define TERMCHART_DSL_GRAPHVIZ_DOT_HPP

#include <cstddef>
#include <string_view>

#include "termchart/core/expected.hpp"
#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/parser_base.hpp"
#include "termchart/export.hpp"

namespace termchart::dsl {

struct DotParseOptions {
    std::size_t max_nodes = 10000;  // 节点上限（超限返回 OutOfRange，不抛异常）
};

[[nodiscard]] TERMCHART_API Result<AstGraph> parse_dot(
    std::string_view source, const DotParseOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

// 典型首行指令；供注册表/自动识别共享，避免两处硬编码字符串。
inline constexpr std::string_view kDotDirective = "digraph";

}  // namespace termchart::dsl

#endif  // TERMCHART_DSL_GRAPHVIZ_DOT_HPP
