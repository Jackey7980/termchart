#include "termchart/dsl/graphviz_dot.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace termchart::dsl {
namespace {

// 防御性边上限：并列节点集是笛卡尔积，|左|×|右| 可能是 O(n²)，
// 没有上限时 fuzz 输入能构造出天量 push（挂死）。与 lexer 的 max_tokens 同量级。
constexpr std::size_t kMaxEdges = 200000;

constexpr std::string_view kDotKeywords[] = {
    "strict", "digraph", "graph", "subgraph", "node", "edge",
};

KeywordSet dot_keywords() noexcept {
    return KeywordSet(std::span<const std::string_view>(kDotKeywords));
}

char lower_ascii(char ch) noexcept {
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
}

bool iequals(std::string_view lhs, std::string_view rhs) noexcept {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (lower_ascii(lhs[i]) != lower_ascii(rhs[i])) {
            return false;
        }
    }
    return true;
}

std::string trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\n' ||
                           text[begin] == '\r')) {
        ++begin;
    }
    while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\n' ||
                           text[end - 1] == '\r')) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

std::string to_upper(std::string_view text) {
    std::string out(text);
    for (char& ch : out) {
        if (ch >= 'a' && ch <= 'z') {
            ch = static_cast<char>(ch - 'a' + 'A');
        }
    }
    return out;
}

// 还原引号字符串里的转义（只处理 DOT 常见的 \" 与 \\）。
std::string unescape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            ++i;
            out.push_back(text[i]);
            continue;
        }
        out.push_back(text[i]);
    }
    return out;
}

bool is_supported_shape(std::string_view shape) noexcept {
    return shape == "box" || shape == "ellipse" || shape == "diamond" || shape == "hexagon" ||
           shape == "cylinder" || shape == "round";
}

// 在**原文切片**上按逗号切属性、按第一个等号切 k/v（引号内的逗号/等号不算分隔符）。
std::vector<AstAttribute> parse_attribute_pairs(std::string_view raw) {
    std::vector<AstAttribute> out;
    const std::size_t n = raw.size();
    std::size_t i = 0;
    while (i < n) {
        while (i < n && (raw[i] == ',' || raw[i] == ' ' || raw[i] == '\t' || raw[i] == '\n' ||
                         raw[i] == '\r')) {
            ++i;
        }
        if (i >= n) {
            break;
        }
        const std::size_t segment_begin = i;
        std::size_t equal_at = std::string_view::npos;
        bool in_quote = false;
        while (i < n) {
            const char ch = raw[i];
            if (in_quote) {
                if (ch == '\\' && i + 1 < n) {
                    i += 2;
                    continue;
                }
                if (ch == '"') {
                    in_quote = false;
                }
                ++i;
                continue;
            }
            if (ch == '"') {
                in_quote = true;
                ++i;
                continue;
            }
            if (ch == ',') {
                break;
            }
            if (ch == '=' && equal_at == std::string_view::npos) {
                equal_at = i;
            }
            ++i;
        }
        const std::size_t segment_end = i;
        if (i < n) {
            ++i;  // 跳过 ','
        }
        const std::string_view segment = raw.substr(segment_begin, segment_end - segment_begin);
        if (equal_at == std::string_view::npos) {
            const std::string key = trim(segment);
            if (!key.empty()) {
                out.push_back(AstAttribute{key, std::string{}});
            }
            continue;
        }
        const std::size_t equal_rel = equal_at - segment_begin;
        std::string key = trim(segment.substr(0, equal_rel));
        std::string value = trim(segment.substr(equal_rel + 1));
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = unescape(std::string_view(value).substr(1, value.size() - 2));
        }
        if (!key.empty()) {
            out.push_back(AstAttribute{std::move(key), std::move(value)});
        }
    }
    return out;
}

class DotParser : public ParserBase {
public:
    DotParser(std::vector<Token> tokens, std::string_view source, const DotParseOptions& options)
        : ParserBase(std::move(tokens), "graphviz-dot", source), options_(options) {}

    Result<AstGraph> run() {
        parse_header();
        if (!failed()) {
            parse_statements();
        }
        if (failed()) {
            return Unexpected<ErrorCode>(diagnostic().code);
        }
        return ast_;
    }

private:
    // ── 头部：strict? digraph|graph [名字] { ─────────────────────────────
    void parse_header() {
        skip_newlines();
        if (match_keyword("strict")) {
            skip_newlines();
        }
        if (check_keyword("digraph")) {
            ast_.directive = advance().text;
        } else if (check_keyword("graph")) {
            ast_.directive = advance().text;
        } else {
            report(ErrorCode::ParseError, "缺少 digraph/graph 头部");
            return;
        }
        skip_newlines();
        if (check_kind(TokenKind::Identifier) || check_kind(TokenKind::Text) ||
            check_kind(TokenKind::Number)) {
            ast_.name = advance().text;  // digraph G { …
        }
        skip_newlines();
        if (!match_punct("{")) {
            report(ErrorCode::ParseError, "头部缺少 {");
        }
    }

    // ── 语句循环 ───────────────────────────────────────────────────────
    void parse_statements() {
        while (true) {
            skip_newlines();
            while (match_punct(";")) {
                skip_newlines();
            }
            if (at_end()) {
                break;
            }
            if (check_punct("}")) {
                (void)advance();
                if (subgraph_depth_ > 0) {
                    --subgraph_depth_;  // 这是某个 subgraph 的收尾，不是本图的
                    continue;
                }
                closed_ = true;
                break;
            }
            const std::size_t before = token_index();
            parse_statement();
            if (failed()) {
                return;
            }
            if (token_index() == before) {
                (void)advance();  // 防挂死：任何语句都必须推进游标
            }
            skip_newlines();
            while (match_punct(";")) {
                skip_newlines();
            }
            if (at_end()) {
                break;
            }
        }
        if (!closed_) {
            report(ErrorCode::ParseError, "缺少闭合的 }");
        }
    }

    void parse_statement() {
        // subgraph 块头：吞掉块头与它的 '{'（块内语句按顶层继续解析）。
        if (check_keyword("subgraph")) {
            (void)advance();
            while (!at_end() && current().kind != TokenKind::Newline) {
                if (match_punct("{")) {
                    ++subgraph_depth_;
                    return;
                }
                (void)advance();
            }
            return;
        }
        // graph/node/edge [attr]：默认属性行，整行忽略（graph 行的 rankdir 仍取用）。
        if ((check_keyword("graph") || check_keyword("node") || check_keyword("edge")) &&
            peek(1).is_punct("[")) {
            const bool is_graph = check_keyword("graph");
            (void)advance();
            std::string raw;
            capture_attribute_list(raw);
            if (failed()) {
                return;
            }
            if (is_graph) {
                apply_rankdir(raw);
            }
            skip_to_end_of_line();
            return;
        }
        // id = value：图属性赋值（只有 rankdir 有意义）。
        if (check_kind(TokenKind::Identifier) && peek(1).is_punct("=")) {
            const std::string id = advance().text;
            (void)advance();  // '='
            std::string value;
            if (check_kind(TokenKind::Identifier) || check_kind(TokenKind::Text) ||
                check_kind(TokenKind::Number)) {
                value = advance().text;
            }
            if (iequals(id, "rankdir")) {
                set_direction(value);
            }
            skip_to_end_of_line();
            return;
        }

        // 节点/边语句：node_set (edgeop node_set)* [attr_list]
        const std::size_t first_edge = ast_.edges.size();
        std::vector<std::string> left;
        if (!parse_node_set(left)) {
            if (!failed()) {
                skip_to_end_of_line();
            }
            return;
        }
        while (check_kind(TokenKind::Arrow)) {
            const Token op = advance();
            const bool has_head = op.text.find('>') != std::string::npos;
            std::vector<std::string> right;
            if (!parse_node_set(right)) {
                if (!failed()) {
                    skip_to_end_of_line();
                }
                return;
            }
            if (!add_edges(left, right, has_head ? ArrowKind::AtTarget : ArrowKind::None)) {
                return;
            }
            left = std::move(right);  // 边链：`a -> b -> c` 的第二条边从 b 出发
        }
        if (check_punct("[")) {
            std::string raw;
            capture_attribute_list(raw);
            if (failed()) {
                return;
            }
            const std::vector<AstAttribute> attrs = parse_attribute_pairs(raw);
            if (ast_.edges.size() > first_edge) {
                apply_edge_attributes(attrs, first_edge);
            } else {
                for (const std::string& key : left) {
                    apply_node_attributes(attrs, key);
                }
            }
        }
        if (!at_statement_end()) {
            report(ErrorCode::ParseError, "语句尾部有多余内容");
            skip_to_end_of_line();
        }
    }

    // ── 节点集：单个节点引用，或 `{ a b c }` ───────────────────────────
    bool parse_node_set(std::vector<std::string>& keys) {
        if (check_punct("{")) {
            (void)advance();
            while (true) {
                skip_newlines();
                while (match_punct(";")) {
                    skip_newlines();
                }
                if (at_end()) {
                    report(ErrorCode::ParseError, "花括号节点集未闭合");
                    return false;
                }
                if (match_punct("}")) {
                    return true;
                }
                const std::size_t before = token_index();
                if (!parse_node_ref(keys)) {
                    if (!failed()) {
                        skip_to_end_of_line();
                    }
                    return false;
                }
                if (token_index() == before) {
                    (void)advance();
                }
            }
        }
        return parse_node_ref(keys);
    }

    bool parse_node_ref(std::vector<std::string>& keys) {
        if (!check_kind(TokenKind::Identifier) && !check_kind(TokenKind::Number) &&
            !check_kind(TokenKind::Text)) {
            report(ErrorCode::ParseError, "期望节点标识符");
            return false;
        }
        const std::string key = advance().text;
        // 端口 `a:port:nw`：跳过（port 名不进 AST，见头文件"不支持"清单）。
        while (check_punct(":") &&
               (peek(1).kind == TokenKind::Identifier || peek(1).kind == TokenKind::Number)) {
            (void)advance();
            (void)advance();
        }
        if (ensure_node(key) == nullptr) {
            return false;
        }
        keys.push_back(key);
        return true;
    }

    bool add_edges(const std::vector<std::string>& left, const std::vector<std::string>& right,
                   ArrowKind arrow) {
        for (const std::string& from : left) {
            for (const std::string& to : right) {
                if (ast_.edges.size() >= kMaxEdges) {
                    report(ErrorCode::OutOfRange, "边数超过上限");
                    return false;
                }
                AstEdge edge;
                edge.from = from;
                edge.to = to;
                edge.arrow = arrow;
                ast_.edges.push_back(std::move(edge));
            }
        }
        return true;
    }

    // ── 属性列表：原文切片 ─────────────────────────────────────────────
    void capture_attribute_list(std::string& raw_out) {
        raw_out.clear();
        const Token open = advance();  // '['
        int depth = 1;
        std::size_t close_offset = 0;
        bool found = false;
        while (!at_end()) {
            const Token token = advance();
            if (token.is_punct("[")) {
                ++depth;
            } else if (token.is_punct("]")) {
                --depth;
                if (depth == 0) {
                    close_offset = token.offset;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            report_at(open, ErrorCode::ParseError, "属性列表未闭合");
            return;
        }
        raw_out = std::string(source().substr(open.offset + 1, close_offset - open.offset - 1));
    }

    void apply_rankdir(std::string_view raw) {
        for (const AstAttribute& attribute : parse_attribute_pairs(raw)) {
            if (iequals(attribute.key, "rankdir")) {
                set_direction(attribute.value);
            }
        }
    }

    void set_direction(std::string_view value) {
        const std::string upper = to_upper(value);
        if (upper == "LR" || upper == "RL" || upper == "TB" || upper == "BT") {
            ast_.direction = upper;
        }
    }

    void apply_node_attributes(const std::vector<AstAttribute>& attrs, const std::string& key) {
        AstNode* node = ast_.find_node(key);
        if (node == nullptr) {
            return;
        }
        for (const AstAttribute& attribute : attrs) {
            if (iequals(attribute.key, "label")) {
                if (!attribute.value.empty()) {
                    node->label = attribute.value;
                }
            } else if (iequals(attribute.key, "shape")) {
                if (is_supported_shape(attribute.value)) {
                    node->shape = attribute.value;
                } else {
                    node->attributes.push_back(attribute);
                }
            } else {
                node->attributes.push_back(attribute);
            }
        }
    }

    void apply_edge_attributes(const std::vector<AstAttribute>& attrs, std::size_t first_edge) {
        for (std::size_t i = first_edge; i < ast_.edges.size(); ++i) {
            AstEdge& edge = ast_.edges[i];
            for (const AstAttribute& attribute : attrs) {
                if (iequals(attribute.key, "label")) {
                    edge.label = attribute.value;
                } else if (iequals(attribute.key, "style")) {
                    if (iequals(attribute.value, "dashed") || iequals(attribute.value, "dotted")) {
                        edge.line_style = "dashed";
                    } else if (iequals(attribute.value, "bold")) {
                        edge.line_style = "thick";
                    }
                    edge.attributes.push_back(attribute);
                } else if (iequals(attribute.key, "dir") && iequals(attribute.value, "both")) {
                    edge.arrow = ArrowKind::Both;
                    edge.attributes.push_back(attribute);
                } else {
                    edge.attributes.push_back(attribute);
                }
            }
        }
    }

    AstNode* ensure_node(const std::string& key) {
        if (AstNode* existing = ast_.find_node(key)) {
            return existing;
        }
        if (ast_.nodes.size() >= options_.max_nodes) {
            report(ErrorCode::OutOfRange, "节点数超过 max_nodes");
            return nullptr;
        }
        return &ast_.ensure_node(key);
    }

    bool at_statement_end() const {
        return at_end() || current().kind == TokenKind::Newline || check_punct(";") ||
               check_punct("}");
    }

    DotParseOptions options_{};
    AstGraph ast_;
    bool closed_ = false;
    std::size_t subgraph_depth_ = 0;
};

}  // namespace

Result<AstGraph> parse_dot(std::string_view source, const DotParseOptions& options,
                           ParseDiagnostic* out_diagnostic) {
    LexerOptions lexer_options;
    lexer_options.comment_prefixes = {"//", "#"};
    const Result<std::vector<Token>> tokens = tokenize(source, dot_keywords(), lexer_options);
    if (!tokens.has_value()) {
        if (out_diagnostic != nullptr) {
            out_diagnostic->code = tokens.error();
            out_diagnostic->message = "词法分析失败";
        }
        return Unexpected<ErrorCode>(tokens.error());
    }
    DotParser parser(*tokens, source, options);
    Result<AstGraph> result = parser.run();
    if (!result.has_value() && out_diagnostic != nullptr) {
        *out_diagnostic = parser.diagnostic();
    }
    return result;
}

}  // namespace termchart::dsl
