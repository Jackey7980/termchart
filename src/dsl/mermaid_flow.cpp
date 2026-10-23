#include "termchart/dsl/mermaid_flow.hpp"

#include <string>
#include <vector>

namespace termchart::dsl {
namespace {

// flowchart 的关键字集合：方向（TD/LR…）按标识符处理，不写死进关键字表。
constexpr std::string_view kFlowchartKeywords[] = {
    "flowchart", "graph", "subgraph", "end", "direction",
    "classdef",  "class",   "click",    "style", "linkstyle",
};

KeywordSet flowchart_keywords() noexcept {
    return KeywordSet(std::span<const std::string_view>(kFlowchartKeywords));
}

struct NodeRef {
    std::string key;
    std::string label;
    std::string shape;
    bool declared = false;  // 语句里显式写了 `A[标签]`
};

ArrowKind arrow_kind_of(std::string_view arrow) noexcept {
    if (arrow == "<-->") {
        return ArrowKind::Both;
    }
    if (arrow == "---" || arrow == "===" || arrow == "-.-" || arrow == "--") {
        return ArrowKind::None;
    }
    return ArrowKind::AtTarget;
}

std::string line_style_of(std::string_view arrow) noexcept {
    if (arrow == "==>" || arrow == "===") {
        return "thick";
    }
    if (arrow == "-.->" || arrow == "-.-") {
        return "dashed";
    }
    return {};
}

std::string trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) {
        ++begin;
    }
    while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t')) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

bool is_direction(std::string_view text) noexcept {
    return text == "TD" || text == "TB" || text == "BT" || text == "LR" || text == "RL" ||
           text == "td" || text == "tb" || text == "bt" || text == "lr" || text == "rl";
}

class FlowchartParser : public ParserBase {
public:
    FlowchartParser(std::vector<Token> tokens, std::string_view source,
                    const FlowchartParseOptions& options)
        : ParserBase(std::move(tokens), "mermaid-flowchart", source), options_(options) {}

    Result<AstGraph> run() {
        parse_header();
        parse_statements(/*stop_at_end_keyword=*/false);
        if (failed()) {
            return Unexpected<ErrorCode>(diagnostic().code);
        }
        return ast_;
    }

private:
    // ── 头部：flowchart TD / graph LR / graph G { ──────────────────────
    void parse_header() {
        skip_newlines();
        if (!(check_keyword("flowchart") || check_keyword("graph"))) {
            report(ErrorCode::ParseError, "缺少 flowchart/graph 头部");
            return;
        }
        ast_.directive = advance().text;
        if (check_kind(TokenKind::Identifier) && peek(1).is_punct("{")) {
            ast_.name = advance().text;  // graph G { ... }
        }
        if (check_kind(TokenKind::Identifier) && is_direction(current().text)) {
            ast_.direction = advance().text;
        }
        (void)match_punct("{");
    }

    // ── 语句循环 ───────────────────────────────────────────────────────
    void parse_statements(bool stop_at_end_keyword) {
        while (!at_end()) {
            skip_newlines();
            if (at_end()) {
                break;
            }
            if (match_punct(";") || match_punct("{")) {
                continue;
            }
            if (check_keyword("end")) {
                if (!stop_at_end_keyword) {
                    report(ErrorCode::ParseError, "多余的 end");
                    (void)advance();
                    continue;
                }
                (void)advance();
                return;
            }
            if (check_keyword("subgraph")) {
                parse_subgraph();
                if (failed()) {
                    return;
                }
                continue;
            }
            if (check_keyword("direction")) {
                (void)advance();
                if (check_kind(TokenKind::Identifier)) {
                    ast_.direction = advance().text;
                }
                skip_to_end_of_line();
                continue;
            }
            if (check_keyword("classdef") || check_keyword("class") || check_keyword("click") ||
                check_keyword("style") || check_keyword("linkstyle")) {
                // 认得出但本切片不做：跳过整行，**不报错**（语义尽失的风险写进了头文件）。
                skip_to_end_of_line();
                continue;
            }
            if (!parse_statement() || failed()) {
                return;
            }
        }
    }

    void parse_subgraph() {
        const Token header = advance();
        if (!options_.allow_subgraph) {
            report_at(header, ErrorCode::Unsupported, "严格模式下不支持 subgraph");
            skip_to_end_of_line();
            return;
        }
        std::size_t end_offset = header.offset + header.length;
        while (end_offset < source().size() && source()[end_offset] != '\n' &&
               source()[end_offset] != '\r') {
            ++end_offset;
        }
        const std::string name =
            trim(source().substr(header.offset + header.length, end_offset - header.offset -
                                                                       header.length));
        group_ = name.empty() ? std::string("subgraph") : name;
        skip_to_end_of_line();

        parse_statements(/*stop_at_end_keyword=*/true);
        group_.clear();  // 无论是否正常闭合，子图作用域都要退出
    }

    // ── 单条语句：节点引用 + 边链 ──────────────────────────────────────
    bool parse_statement() {
        NodeRef first;
        if (!parse_node_ref(first)) {
            return false;
        }
        register_node(first);

        bool saw_edge = false;
        while (check_kind(TokenKind::Arrow)) {
            saw_edge = true;
            const Token arrow = advance();
            // 必须自持字符串：advance() 返回临时 Token，其 text 成员的视图会当场悬空
            // （ASan stack-use-after-scope，tests/unit/dsl/test_dsl_mermaid.cpp:238）。
            std::string arrow_text = arrow.text;
            std::string label;

            // `A -- 文字 --> B`：开箭头 + 文字 + 闭箭头
            if ((arrow.text == "--" || arrow.text == "-." || arrow.text == "==") &&
                !check_punct("|") && !check_punct("[")) {
                if (!at_end() && current().kind != TokenKind::Newline &&
                    !check_kind(TokenKind::Arrow)) {
                    label = current().text;
                    (void)advance();
                }
                if (check_kind(TokenKind::Arrow)) {
                    arrow_text = advance().text;  // 闭箭头决定语义
                }
            }
            // `-->|文字|`
            if (match_punct("|")) {
                if (!check_punct("|")) {
                    label = current().text;
                    (void)advance();
                }
                if (!match_punct("|")) {
                    report(ErrorCode::ParseError, "边标签缺少闭合的 |");
                    return false;
                }
            }

            NodeRef target;
            if (!parse_node_ref(target)) {
                return false;
            }
            register_node(target);

            AstEdge edge;
            edge.from = first.key;
            edge.to = target.key;
            edge.label = std::move(label);
            edge.arrow = arrow_kind_of(arrow_text);
            edge.line_style = line_style_of(arrow_text);
            ast_.edges.push_back(std::move(edge));
            first = target;  // 边链：`A --> B --> C` 的第二条边从 B 出发
        }

        if (!saw_edge && current().kind == TokenKind::Newline) {
            return true;  // 纯声明，行尾即收口
        }
        return expect_end_of_statement();
    }

    // ── 节点引用：key + 可选形状/标签 ─────────────────────────────────
    bool parse_node_ref(NodeRef& out) {
        if (!check_kind(TokenKind::Identifier) && !check_kind(TokenKind::Number)) {
            report(ErrorCode::ParseError, "期望节点标识符");
            return false;
        }
        out.key = advance().text;
        out.label = out.key;

        if (check_punct("(") && peek(1).is_punct("(")) {
            out.shape = "circle";
            return capture_bracketed("((", "))", out);
        }
        if (check_punct("(") && peek(1).is_punct("[")) {
            out.shape = "stadium";
            return capture_bracketed("([", "])", out);
        }
        if (check_punct("{")) {
            if (peek(1).is_punct("{")) {
                out.shape = "hexagon";
                return capture_bracketed("{{", "}}", out);
            }
            out.shape = "diamond";
            return capture_bracketed("{", "}", out);
        }
        if (check_punct("(")) {
            out.shape = "round";
            return capture_bracketed("(", ")", out);
        }
        if (check_punct("[")) {
            out.shape = "box";
            return capture_bracketed("[", "]", out);
        }
        if (check_punct(">")) {
            out.shape = "asymmetric";
            return capture_bracketed(">", "]", out);
        }
        return true;  // 裸引用
    }

    // 括号内是逐字文本（可含空格/标点/中文）：按**原文切片**取标签，
    // 再把 token 游标推到闭合括号之后。
    bool capture_bracketed(std::string_view open, std::string_view close, NodeRef& ref) {
        const Token open_token = current();
        const std::size_t content_begin = open_token.offset + open.size();
        const std::string_view raw = source();
        std::size_t cursor = content_begin;
        int depth = 1;
        std::size_t close_begin = std::string_view::npos;
        while (cursor < raw.size()) {
            const char ch = raw[cursor];
            if (ch == '"' || ch == '\'') {
                const char quote = ch;
                ++cursor;
                while (cursor < raw.size() && raw[cursor] != quote) {
                    if (raw[cursor] == '\\' && cursor + 1 < raw.size()) {
                        ++cursor;
                    }
                    ++cursor;
                }
                ++cursor;
                continue;
            }
            if (raw.compare(cursor, open.size(), open) == 0) {
                ++depth;
                cursor += open.size();
                continue;
            }
            if (raw.compare(cursor, close.size(), close) == 0) {
                --depth;
                if (depth == 0) {
                    close_begin = cursor;
                    break;
                }
                cursor += close.size();
                continue;
            }
            ++cursor;
        }
        if (close_begin == std::string_view::npos) {
            report_at(open_token, ErrorCode::ParseError, "括号未闭合");
            skip_to_end_of_line();
            return false;
        }
        ref.label = trim(raw.substr(content_begin, close_begin - content_begin));
        if (ref.label.empty()) {
            ref.label = ref.key;
        }
        ref.declared = true;

        const std::size_t after_close = close_begin + close.size();
        while (current().kind != TokenKind::End && current().offset < after_close) {
            (void)advance();
        }
        return true;
    }

    void register_node(const NodeRef& ref) {
        if (AstNode* existing = ast_.find_node(ref.key)) {
            if (ref.declared) {
                existing->label = ref.label;
                if (!ref.shape.empty()) {
                    existing->shape = ref.shape;
                }
            }
            // 先被边引用、后出现在 subgraph 里的节点：归属以**最后一次提及**为准。
            // 这是 mermaid 的实际语义（节点可以在子图里被"再声明"）。
            if (!group_.empty()) {
                set_group(*existing, group_);
            }
            return;
        }
        if (ast_.nodes.size() >= options_.max_nodes) {
            report(ErrorCode::OutOfRange, "节点数超过 max_nodes");
            return;
        }
        AstNode node;
        node.key = ref.key;
        node.label = ref.label.empty() ? ref.key : ref.label;
        node.shape = ref.shape;
        if (!group_.empty()) {
            node.attributes.push_back(AstAttribute{"group", group_});
        }
        ast_.nodes.push_back(std::move(node));
    }

    static void set_group(AstNode& node, const std::string& group) {
        for (AstAttribute& attribute : node.attributes) {
            if (attribute.key == "group") {
                attribute.value = group;
                return;
            }
        }
        node.attributes.push_back(AstAttribute{"group", group});
    }

    FlowchartParseOptions options_{};
    AstGraph ast_;
    std::string group_;
};

}  // namespace

Result<AstGraph> parse_flowchart(std::string_view source, const FlowchartParseOptions& options,
                                 ParseDiagnostic* out_diagnostic) {
    const Result<std::vector<Token>> tokens = tokenize(source, flowchart_keywords());
    if (!tokens.has_value()) {
        if (out_diagnostic != nullptr) {
            out_diagnostic->code = tokens.error();
            out_diagnostic->message = "词法分析失败";
        }
        return Unexpected<ErrorCode>(tokens.error());
    }
    FlowchartParser parser(*tokens, source, options);
    Result<AstGraph> result = parser.run();
    if (!result.has_value() && out_diagnostic != nullptr) {
        *out_diagnostic = parser.diagnostic();
    }
    return result;
}

}  // namespace termchart::dsl
