// 33_mermaid_flowchart.cpp —— Mermaid 文本 → 流程图（阶段 5 出口条件之一）
//
// 两条路的对比（本示例演示的正是第二条）：
//   14_flowchart.cpp：手工 `add_node`/`add_edge` 建图；
//   本示例：直接喂 Mermaid 文本，dsl 解析成 AST → to_graph → 同一套布局与渲染。
// 关键点：**dsl 不画图**，它只产出 Graph；渲染完全复用 graph/diagrams 那条链路，
// 因此 DSL 图与手工图的观感、可导出性、可交互性完全一致（同一份 render_flowchart）。
#include <cstdio>
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/api/quick.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/style.hpp"
#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/mermaid.hpp"
#include "termchart/graph/diagrams/flowchart.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/term/caps.hpp"
#include "termchart/term/size.hpp"

namespace {

constexpr std::string_view kMermaid = R"(flowchart LR
    A[输入 CSV] --> B{列类型?}
    B -->|数值| C[统计]
    B -- 文本 --> D[词频]
    C --> E([图表])
    D --> E
    subgraph 输出
        E
    end
)";

}  // namespace

int main() {
    // ── 1. 解析：文本 → AST（失败时给出定位，不抛异常） ────────────────
    termchart::dsl::ParseDiagnostic diagnostic;
    termchart::dsl::MermaidOptions options;
    const auto ast = termchart::dsl::parse_mermaid(kMermaid, options, &diagnostic);
    if (!ast.has_value()) {
        std::fprintf(stderr, "解析失败（%s）: 第 %zu 行第 %zu 列: %s\n",
                     termchart::to_string(ast.error()), diagnostic.line, diagnostic.column,
                     diagnostic.message.c_str());
        return 1;
    }
    if (termchart::dsl::detect_diagram(kMermaid) != termchart::dsl::MermaidDiagram::Flowchart) {
        std::fprintf(stderr, "首行指令识别错误\n");
        return 1;
    }

    // ── 2. AST → Graph（唯一的适配点） ─────────────────────────────────
    auto graph = termchart::dsl::to_graph(*ast);
    if (!graph.has_value()) {
        std::fprintf(stderr, "AST 转 Graph 失败: %s\n", termchart::to_string(graph.error()));
        return 1;
    }

    // ── 3. 画布尺寸随终端（无终端时退回 80×24） ────────────────────────
    const termchart::term::TerminalSize size = termchart::term::query_size();
    const int cols = size.cols == 0 ? 80 : size.cols;
    const int rows = size.rows == 0 ? 24 : size.rows;
    termchart::canvas::Canvas canvas(static_cast<std::uint32_t>(cols),
                                     static_cast<std::uint32_t>(rows > 6 ? rows - 2 : rows));

    // ── 4. 复用与手工图完全相同的渲染入口 ─────────────────────────────
    termchart::graph::FlowchartOptions render_options;
    render_options.layout.min_width = 10.0;
    render_options.layout.h_gap = 3.0;
    render_options.node_style = termchart::Style{}.set_fg(termchart::Color::ansi16(6));
    render_options.edge_style = termchart::Style{}.set_fg(termchart::Color::ansi16(8));
    (void)termchart::graph::render_flowchart(*graph, canvas, render_options);

    const termchart::Status header = termchart::api::print_line(
        "mermaid flowchart → graph → canvas → stdout",
        termchart::Style{}.set_fg(termchart::Color::rgb(86, 197, 154)).set_attr(termchart::Attr::Bold));

    termchart::render::Renderer renderer;
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    const termchart::Status status = termchart::api::write_stdout(bytes);
    (void)termchart::api::flush_stdout();
    return (header == termchart::ErrorCode::Ok && status == termchart::ErrorCode::Ok) ? 0 : 1;
}
