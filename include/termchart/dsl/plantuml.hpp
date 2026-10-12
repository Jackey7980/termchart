// plantuml.hpp —— PlantUML 类图语法（L17）
//
// 支持范围（**明确列出，避免"以为支持"**）：
//   ✅ 包裹：`@startuml [名字]` … `@enduml`（任一端缺失 → ParseError）
//   ✅ 注释：`'` 行注释、单行 `/' … '/` 块注释（多行块注释见"不支持"）
//   ✅ 类声明：`class 名称`、`class 名称 as 别名`、`class 名称 { …体… }`，
//      另把 `interface` / `enum` / `abstract class` 当同类处理（都建一个节点）
//   ✅ 类体：`{` … `}` 内**每一行原文**落进该节点 attributes（键 "member"，可多条），
//      **不解析**成员语法（可见性/类型/方法一律原样保留）
//   ✅ 关系：`-->`、`--`、`--|>`、`<|--`、`..>`、`..|>`、`*--`、`o--`（另收 `--*`/`--o`/`<|..`）
//      —— 语义写进 AstEdge::attributes 键 "relation"：
//         inheritance / composition / aggregation / association / dependency；
//         虚线（`..`）写进 AstEdge::line_style="dashed"；箭头统一用 ArrowKind；
//         `A <|-- B` 这类"箭头在左端"的写法会**交换** from/to（B 继承 A ⇒ from=B,to=A）
//   ✅ 关系标签 `A --> B : label`（取 `:` 到行尾的原文）
//   ✅ 多重度 `A "1" --> "*" B`（写进 edge attributes 键 from_multiplicity / to_multiplicity）
//   ⛔ 认出但**忽略**（不改变图结构）：`note`（单行 `note … : …` 与块 `note …`/`end note`）、
//      `skinparam`、`title`/`caption`/`legend`、`package`/`namespace` 块头（块内语句仍按顶层解析）、
//      以及一切既不是类声明也不是已识别关系的行——**整行跳过、继续**
//   ⛔ **不支持**：多行 `/' … '/` 块注释（词法层按行注释处理，内部行会被当作未识别行）、
//      泛型 `Foo<T>`、构造型 `<<…>>`、`hide/show` 的过滤语义、组件/时序/活动等其他 PlantUML 图族
//
// 为什么宽松：PlantUML 方言极杂且大量语法与布局有关。这里只承诺"类图的关系骨架"，
// 其余一律不猜——**认不出就整行跳过**，绝不让未识别的行静默改变已解析出的节点/边。
//
// 诊断：失败返回解析器错误码（ParseError 语法错 / OutOfRange 超 max_nodes），
//       可选地把首个诊断写到 out_diagnostic；任何路径都**不抛异常**。
#ifndef TERMCHART_DSL_PLANTUML_HPP
#define TERMCHART_DSL_PLANTUML_HPP

#include <cstddef>
#include <string_view>

#include "termchart/core/expected.hpp"
#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/parser_base.hpp"
#include "termchart/export.hpp"

namespace termchart::dsl {

struct PlantUmlParseOptions {
    std::size_t max_nodes = 10000;  // 节点上限（超限返回 OutOfRange，不抛异常）
};

[[nodiscard]] TERMCHART_API Result<AstGraph> parse_plantuml(
    std::string_view source, const PlantUmlParseOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

// 典型首行指令；供注册表/自动识别共享，避免两处硬编码字符串。
inline constexpr std::string_view kPlantUmlDirective = "@startuml";

}  // namespace termchart::dsl

#endif  // TERMCHART_DSL_PLANTUML_HPP
