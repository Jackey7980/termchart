// mermaid_flow.hpp —— Mermaid flowchart 语法（L17）
//
// 支持范围（**明确列出，避免"以为支持"**）：
//   ✅ 头部 `flowchart TD` / `graph LR` / `graph G { }`（图名与方向）
//   ✅ 节点：裸引用 `A`、`A[标签]`、`A(标签)`、`A([标签])`、`A((标签))`、
//      `A{标签}`、`A{{标签}}`、`A>标签]`
//   ✅ 边：`-->` `---` `-.->` `-.-` `==>` `===` `<-->` `--x` `--o`
//   ✅ 边标签两种写法：`A -->|文字| B` 与 `A -- 文字 --> B`
//   ✅ 边链：`A --> B --> C`
//   ✅ 子图：`subgraph 名称` … `end`（子图名记为节点属性 `group`，不做嵌套布局）
//   ⛔ 忽略但**不报错**的指令：`classDef` / `class` / `click` / `style` / `linkStyle` /
//      `direction`（后者的值仍会被记录到 AstGraph::direction）
//   ⛔ 本切片不支持的：节点 `:::class` 后缀、`&` 并列节点、`--o`/`--x` 的
//      方向语义细节（统一按 AtTarget 处理）
//
// 诊断：解析失败返回解析器的错误码，可选地把首个诊断写到 out_diagnostic。
#ifndef TERMCHART_DSL_MERMAID_FLOW_HPP
#define TERMCHART_DSL_MERMAID_FLOW_HPP

#include <cstddef>
#include <string_view>

#include "termchart/core/expected.hpp"
#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/parser_base.hpp"
#include "termchart/export.hpp"

namespace termchart::dsl {

struct FlowchartParseOptions {
    bool allow_subgraph = true;   // 关掉后 subgraph 视为语法错误（严格模式）
    std::size_t max_nodes = 10000;
};

[[nodiscard]] TERMCHART_API Result<AstGraph> parse_flowchart(
    std::string_view source, const FlowchartParseOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

// 方言描述符用到的典型指令；供注册表/自动识别共享，避免两处硬编码字符串。
inline constexpr std::string_view kFlowchartDirective = "flowchart";

}  // namespace termchart::dsl

#endif  // TERMCHART_DSL_MERMAID_FLOW_HPP
