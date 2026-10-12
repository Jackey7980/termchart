# DSL 语法参考

## 方言与入口（dsl/，全部已落地）

| 头文件 | 方言 | 产物 |
| --- | --- | --- |
| mermaid.hpp | Mermaid 总入口（识别 + 分发） | `Result<AstGraph>`（序列图除外） |
| mermaid_flow.hpp | Mermaid flowchart / graph | `AstGraph` → `to_graph()` |
| mermaid_class.hpp | Mermaid classDiagram | `AstGraph` |
| mermaid_state.hpp | Mermaid stateDiagram(-v2) | `AstGraph` |
| mermaid_sequence.hpp | Mermaid sequenceDiagram | 序列图时间轴模型（`parse_sequence()`） |
| plantuml.hpp | PlantUML | 按头文件为准 |
| graphviz_dot.hpp | Graphviz DOT | `AstGraph` |
| table_markdown.hpp | Markdown 表格 | 表格模型 |
| csv_sniff.hpp | CSV 方言嗅探 | 分隔符/表头判定 |
| d2.hpp | D2 | 按头文件为准 |
| ast.hpp | 中间表示 `AstGraph` | — |
| lexer.hpp / parser_base.hpp | 词法/解析基础设施 | — |
| dsl_registry.hpp | 方言注册表 | — |

## Mermaid 用法

```cpp
#include "termchart/dsl/mermaid.hpp"

using namespace termchart::dsl;

// 1. 识别（纯文本、不分配，可反复调用）
MermaidDiagram d = detect_diagram(source);   // Flowchart/Sequence/Class/State/Unknown

// 2. 解析
MermaidOptions opt;                    // max_nodes=10000, allow_subgraph=true
ParseDiagnostic diag;
Result<AstGraph> ast = parse_mermaid(source, opt, &diag);
if (!ast) { /* diag 携带行号与原因 */ }

// 3. 落到图可视化管线
termchart::graph::Graph g = ast.value().to_graph();
// → render_flowchart(g, canvas)  （graph/diagrams/flowchart.hpp）
```

自动识别规则（首个非空、非注释行）：`flowchart|graph` → Flowchart；
`sequenceDiagram` → Sequence；`classDiagram` → Class；
`stateDiagram(-v2)` → State；其它 → Unknown。

## 设计要点

- **识别与解析分离**：`detect_diagram` 纯文本零分配；
- **不假装解析成功**：序列图不在 mermaid.hpp 的产物类型里（时间轴模型 ≠ 图模型），
  入口对它返回 `Unsupported` 并在诊断里说明；
- 错误通道是 `Result<T>` + `ParseDiagnostic`（行号 + 原因），不抛异常。

## 示例

examples/33_mermaid_flowchart：Mermaid 文本 → 图 → 终端流程图。

## 未做

- 本文档不逐方言列语法细节（以各头文件顶部注释为准）；
- DSL 语句级语法的完整 EBNF 未单独成文。
