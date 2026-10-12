// mermaid_state.hpp —— Mermaid stateDiagram（L17）
//
// 状态机天然是**图**（状态=节点、迁移=边），因此本方言的产物就是 `AstGraph`，
// 可以直接 `to_graph()` 落到 `graph::Graph` 复用分层布局与正交路由。
//
// ✅ 支持
//   `stateDiagram` / `stateDiagram-v2` 头部、`direction LR` 等方向
//   `A --> B`、`A --> B : 事件[守卫]/动作`、`A -- B`（等价于 -->）
//   `[*] --> A`（A 记为初始态，节点属性 `initial=true`）
//   `A --> [*]`（A 记为终态，节点属性 `final=true`）
//   `state X { … }` 组合态：X 记 `composite=true`，块内状态记 `parent=X`（不做嵌套布局）
//   `state X as 显示名`、`state "显示名" as X`
//   `note left of X : …` / `note right of X : …` → 节点属性 `note`
// ⛔ 认出但忽略（不影响图结构）：`note` 的其它形态、`classDef`/`class`/`style`/`click`
// ⛔ 不支持：`--` 上的并发区域（`--` 分行）、`<<choice>>`/`<<fork>>` 等构造型语义
#ifndef TERMCHART_DSL_MERMAID_STATE_HPP
#define TERMCHART_DSL_MERMAID_STATE_HPP

#include <cstddef>
#include <string_view>

#include "termchart/core/expected.hpp"
#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/parser_base.hpp"
#include "termchart/export.hpp"

namespace termchart::dsl {

struct StateParseOptions {
    std::size_t max_nodes = 10000;
};

[[nodiscard]] TERMCHART_API Result<AstGraph> parse_state_diagram(
    std::string_view source, const StateParseOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

inline constexpr std::string_view kStateDiagramDirective = "stateDiagram";

}  // namespace termchart::dsl

#endif  // TERMCHART_DSL_MERMAID_STATE_HPP
