// fuzz_mermaid.cpp —— Mermaid 三个图种解析的健壮性（规格 fuzz 目标之一）
//
// 关注点：解析器面对**任意字节**都必须
//   * 不崩、不挂死（任何循环都要让游标前进）；
//   * 不抛异常（错误走 Result + ParseDiagnostic）；
//   * 不产生越界的图结构（边两端必须是已登记节点，`to_graph` 也要能接住）。
// 输入既包含完全随机的字节，也包含"把合法 Mermaid 截断/篡改"的变形语料——
// 后者才是真实 bug 的高发区（未闭合括号、缺箭头、孤立的 `[*]`）。
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "fuzz_common.hpp"
#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/mermaid.hpp"
#include "termchart/dsl/mermaid_sequence.hpp"

namespace {

void target(const std::uint8_t* data, std::size_t size) {
    const auto input = std::string_view(reinterpret_cast<const char*>(data), size);

    // 1) 图模型一族：识别 → 分发 → 落成 graph::Graph
    const auto ast = termchart::dsl::parse_mermaid(input);
    if (ast.has_value()) {
        (void)termchart::dsl::to_graph(*ast);
    }

    // 2) 序列图：独立类型，同样不能崩
    const auto sequence = termchart::dsl::parse_sequence(input);
    if (sequence.has_value()) {
        for (const auto& message : sequence->messages) {
            (void)termchart::dsl::message_kind_name(message.kind);
        }
    }

    // 3) 识别本身也吃一遍（它是所有入口的前置）
    (void)termchart::dsl::detect_diagram(input);
}

}  // namespace

TERMCHART_FUZZ_MAIN(target)
