// mermaid_class.hpp —— Mermaid classDiagram（L17）
//
// 类图是"图 + 节点内容"，因此产物仍是 `AstGraph`：
//   * 类 = 节点，类体与方法逐行原文进节点属性 `member`（多条）；
//   * 关系 = 边，**语义写进边属性 `relation`**：inheritance / composition /
//     aggregation / association / dependency / realization；
//   * 布局仍交给 `graph::layout`，渲染交给 `graph/diagrams`（本层不画东西）。
//
// ✅ 支持
//   `classDiagram` 头部；`direction LR`
//   `class 名称`、`class 名称 { … }`、`class 名称 as 别名`、`class 名称["显示名"]`
//   `A <|-- B`、`A --|> B`、`A *-- B`、`A o-- B`、`A --> B`、`A ..> B`、`A ..|> B`
//   `A --> B : 标签`、`A "1" --> "*" B`（多重度进边属性 `from_multiplicity` / `to_multiplicity`）
//   `A : +成员()`（单行成员补写）
//   `<<interface>>` / `<<abstract>>` 构造型 → 节点属性 `stereotype`
// ⛔ 认出但忽略：`note for`、`style`/`classDef`/`class` 着色语句、`namespace`/`package`（记 `namespace` 属性）
// ⛔ 不支持：泛型 `~T~` 的语义解释（原文保留在成员文本里）、关系方向的双箭头细节
//
// 方向语义（最容易搞反，测试锁死）：
//   `A <|-- B` → B 继承 A，边 from=B to=A（三角指向父类 A）
//   `A --|> B` → A 继承 B，边 from=A to=B
#ifndef TERMCHART_DSL_MERMAID_CLASS_HPP
#define TERMCHART_DSL_MERMAID_CLASS_HPP

#include <cstddef>
#include <string_view>

#include "termchart/core/expected.hpp"
#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/parser_base.hpp"
#include "termchart/export.hpp"

namespace termchart::dsl {

struct ClassParseOptions {
    std::size_t max_nodes = 10000;
};

[[nodiscard]] TERMCHART_API Result<AstGraph> parse_class_diagram(
    std::string_view source, const ClassParseOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

inline constexpr std::string_view kClassDiagramDirective = "classDiagram";

}  // namespace termchart::dsl

#endif  // TERMCHART_DSL_MERMAID_CLASS_HPP
