#include "termchart/dsl/mermaid.hpp"

#include "termchart/dsl/mermaid_flow.hpp"

namespace termchart::dsl {
namespace {

// 取首个"非空、非注释"行的起始位置；没有则返回 npos。
std::size_t first_statement_offset(std::string_view source) noexcept {
    std::size_t pos = 0;
    while (pos < source.size()) {
        // 跳过空行与行首空白
        while (pos < source.size() && (source[pos] == ' ' || source[pos] == '\t')) {
            ++pos;
        }
        if (pos >= source.size()) {
            break;
        }
        if (source[pos] == '\n' || source[pos] == '\r') {
            ++pos;
            continue;
        }
        if (source.compare(pos, 2, "%%") == 0) {  // mermaid 注释
            while (pos < source.size() && source[pos] != '\n') {
                ++pos;
            }
            continue;
        }
        return pos;
    }
    return std::string_view::npos;
}

bool starts_with_word(std::string_view text, std::string_view word) noexcept {
    if (text.size() < word.size() || text.compare(0, word.size(), word) != 0) {
        return false;
    }
    if (text.size() == word.size()) {
        return true;
    }
    // '-' 是给 `stateDiagram-v2` 这类带后缀的指令留的口子。
    const char next = text[word.size()];
    return next == ' ' || next == '\t' || next == '\n' || next == '\r' || next == '{' ||
           next == '-';
}

}  // namespace

MermaidDiagram detect_diagram(std::string_view source) noexcept {
    const std::size_t offset = first_statement_offset(source);
    if (offset == std::string_view::npos) {
        return MermaidDiagram::Unknown;
    }
    const std::string_view rest = source.substr(offset);
    if (starts_with_word(rest, "flowchart") || starts_with_word(rest, "graph")) {
        return MermaidDiagram::Flowchart;
    }
    if (starts_with_word(rest, "sequenceDiagram")) {
        return MermaidDiagram::Sequence;
    }
    if (starts_with_word(rest, "classDiagram")) {
        return MermaidDiagram::Class;
    }
    if (starts_with_word(rest, "stateDiagram")) {
        return MermaidDiagram::State;
    }
    return MermaidDiagram::Unknown;
}

const char* diagram_name(MermaidDiagram diagram) noexcept {
    switch (diagram) {
        case MermaidDiagram::Flowchart:
            return "flowchart";
        case MermaidDiagram::Sequence:
            return "sequenceDiagram";
        case MermaidDiagram::Class:
            return "classDiagram";
        case MermaidDiagram::State:
            return "stateDiagram";
        case MermaidDiagram::Unknown:
            break;
    }
    return "unknown";
}

Result<AstGraph> parse_mermaid(std::string_view source, const MermaidOptions& options,
                               ParseDiagnostic* out_diagnostic) {
    switch (detect_diagram(source)) {
        case MermaidDiagram::Flowchart: {
            FlowchartParseOptions flowchart_options;
            flowchart_options.allow_subgraph = options.allow_subgraph;
            flowchart_options.max_nodes = options.max_nodes;
            return parse_flowchart(source, flowchart_options, out_diagnostic);
        }
        case MermaidDiagram::Sequence:
        case MermaidDiagram::Class:
        case MermaidDiagram::State:
            if (out_diagnostic != nullptr) {
                out_diagnostic->code = ErrorCode::NotImplemented;
                out_diagnostic->message = "该 Mermaid 图种尚未实现（当前仅 flowchart）";
            }
            return Unexpected<ErrorCode>(ErrorCode::NotImplemented);
        case MermaidDiagram::Unknown:
            break;
    }
    if (out_diagnostic != nullptr) {
        out_diagnostic->code = ErrorCode::ParseError;
        out_diagnostic->message = "无法识别的 Mermaid 首行指令";
    }
    return Unexpected<ErrorCode>(ErrorCode::ParseError);
}

}  // namespace termchart::dsl
