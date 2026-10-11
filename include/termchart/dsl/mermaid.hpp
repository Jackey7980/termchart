// mermaid.hpp —— Mermaid 总入口（L17）
//
// 职责只有两件：
//   1. **识别**首行指令，判断这是哪种图（docs/guide/dsl_reference.md 的表格口径）；
//   2. 分发到对应方言解析器。
// 本切片只实现 flowchart；其余识别得到但返回 NotImplemented，**绝不假装解析成功**。
//
// 自动识别规则（首个非空、非注释行）：
//   flowchart / graph   → Flowchart
//   sequenceDiagram     → Sequence
//   classDiagram        → Class
//   stateDiagram(-v2)   → State
//   其它                → Unknown（由调用方决定是报错还是继续探测别的方言）
#ifndef TERMCHART_DSL_MERMAID_HPP
#define TERMCHART_DSL_MERMAID_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "termchart/core/expected.hpp"
#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/parser_base.hpp"
#include "termchart/export.hpp"

namespace termchart::dsl {

enum class MermaidDiagram : std::uint8_t {
    Unknown = 0,
    Flowchart,
    Sequence,
    Class,
    State,
};

struct MermaidOptions {
    std::size_t max_nodes = 10000;
    bool allow_subgraph = true;
};

// 纯文本识别，不解析、不分配（可在探测循环里反复调用）。
[[nodiscard]] TERMCHART_API MermaidDiagram detect_diagram(std::string_view source) noexcept;
[[nodiscard]] TERMCHART_API const char* diagram_name(MermaidDiagram diagram) noexcept;

[[nodiscard]] TERMCHART_API Result<AstGraph> parse_mermaid(
    std::string_view source, const MermaidOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

}  // namespace termchart::dsl

#endif  // TERMCHART_DSL_MERMAID_HPP
