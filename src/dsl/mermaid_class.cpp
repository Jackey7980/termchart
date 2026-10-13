#include "termchart/dsl/mermaid_class.hpp"

#include <string>
#include <vector>

namespace termchart::dsl {
namespace {

constexpr std::string_view kClassKeywords[] = {
    "classdiagram", "class", "direction", "note", "style", "classdef",
    "click",        "link",  "cssclass",  "as",   "namespace", "package",
};

KeywordSet class_keywords() noexcept {
    return KeywordSet(std::span<const std::string_view>(kClassKeywords));
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

// 关系表：按箭头串长度降序匹配（`<|--` 必须先于 `<|` 与 `--`）。
struct RelationPattern {
    std::string_view text;
    std::string_view relation;   // 写进边属性
    std::string line_style;      // "" 实线 / "dashed"
    bool reversed = false;       // true 表示"关系方向与书写方向相反"（A <|-- B）
    bool at_target = true;
};

constexpr RelationPattern kRelations[] = {
    {"<|..", "realization", "dashed", true, true},
    {"..|>", "realization", "dashed", false, true},
    {"<|--", "inheritance", "", true, true},
    {"--|>", "inheritance", "", false, true},
    {"*--", "composition", "", false, true},
    {"--*", "composition", "", true, true},
    {"o--", "aggregation", "", false, true},
    {"--o", "aggregation", "", true, true},
    {"..>", "dependency", "dashed", false, true},
    {"<..", "dependency", "dashed", true, true},
    {"-->", "association", "", false, true},
    {"<--", "association", "", true, true},
    {"--", "association", "", false, false},
};

const RelationPattern* match_relation(std::string_view text) noexcept {
    for (const RelationPattern& pattern : kRelations) {
        if (pattern.text == text) {
            return &pattern;
        }
    }
    return nullptr;
}

class ClassParser : public ParserBase {
public:
    ClassParser(std::vector<Token> tokens, std::string_view source,
                const ClassParseOptions& options)
        : ParserBase(std::move(tokens), "mermaid-class", source), options_(options) {}

    Result<AstGraph> run() {
        parse_header();
        parse_statements();
        if (failed()) {
            return Unexpected<ErrorCode>(diagnostic().code);
        }
        return ast_;
    }

private:
    void parse_header() {
        skip_newlines();
        if (!check_keyword("classdiagram")) {
            report(ErrorCode::ParseError, "缺少 classDiagram 头部");
            return;
        }
        ast_.directive = advance().text;
    }

    void parse_statements() {
        while (!at_end()) {
            skip_newlines();
            if (at_end()) {
                break;
            }
            if (match_punct(";")) {
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
            if (check_keyword("class")) {
                parse_class_declaration();
                if (failed()) {
                    return;
                }
                continue;
            }
            if (check_keyword("note") || check_keyword("style") || check_keyword("classdef") ||
                check_keyword("click") || check_keyword("link") || check_keyword("cssclass")) {
                skip_to_end_of_line();
                continue;
            }
            if (!parse_relation_or_member()) {
                if (failed()) {
                    return;
                }
                skip_to_end_of_line();
            }
        }
    }

    // `class 名称` / `class 名称 { … }` / `class 名称 as 别名` / `class 名称["显示名"]`
    void parse_class_declaration() {
        (void)advance();  // class
        std::string name;
        if (check_kind(TokenKind::Identifier)) {
            name = advance().text;
        } else if (check_kind(TokenKind::Text)) {
            name = advance().text;
        } else {
            report(ErrorCode::ParseError, "class 后缺少类名");
            skip_to_end_of_line();
            return;
        }
        std::string label = name;
        if (check_keyword("as")) {
            (void)advance();
            if (check_kind(TokenKind::Identifier) || check_kind(TokenKind::Text)) {
                label = advance().text;
            }
        } else if (check_punct("[")) {
            label = capture_bracketed_text("[", "]");
        }
        AstNode& node = ensure(name, label);

        // `<<interface>>` 构造型：写在类名之后、类体之前。
        while (check_punct("<") || check_punct(">")) {
            const std::size_t start = current().offset;
            while (!at_end() && !check_punct("{")) {
                if (current().kind == TokenKind::Identifier || current().kind == TokenKind::Punctuation) {
                    if (current().text == "interface" || current().text == "abstract" ||
                        current().text == "enumeration" || current().text == "service") {
                        node.attributes.push_back(AstAttribute{"stereotype", current().text});
                    }
                }
                if (current().offset > start + 32) {
                    break;  // 防御：构造型不该跨很多 token
                }
                (void)advance();
            }
            break;
        }

        if (!match_punct("{")) {
            skip_to_end_of_line();
            return;
        }
        // 类体：逐行原文进 member（不解析成员语法）
        while (!at_end() && !check_punct("}")) {
            if (current().kind == TokenKind::Newline) {
                (void)advance();
                continue;
            }
            const std::size_t start = current().offset;
            std::size_t end = start;
            while (end < source().size() && source()[end] != '\n' && source()[end] != '\r') {
                ++end;
            }
            const std::string member = trim(source().substr(start, end - start));
            if (!member.empty()) {
                node.attributes.push_back(AstAttribute{"member", member});
            }
            skip_to_end_of_line();
            if (current().kind == TokenKind::Newline) {
                (void)advance();
            }
        }
        if (!match_punct("}")) {
            report(ErrorCode::ParseError, "类体缺少闭合 }");
            return;
        }
    }

    // `A --> B` / `A : 成员` / 裸类名
    bool parse_relation_or_member() {
        if (!(check_kind(TokenKind::Identifier) || check_kind(TokenKind::Text))) {
            report(ErrorCode::ParseError, "期望类名或关系");
            return false;
        }
        const std::string left = advance().text;

        // `A : +成员()`：单行成员补写
        if (match_punct(":")) {
            AstNode& node = ensure(left, left);
            const std::size_t start = current().offset;
            std::size_t end = start;
            while (end < source().size() && source()[end] != '\n' && source()[end] != '\r') {
                ++end;
            }
            const std::string member = trim(source().substr(start, end - start));
            if (!member.empty()) {
                node.attributes.push_back(AstAttribute{"member", member});
            }
            skip_to_end_of_line();
            return true;
        }

        std::string from_multiplicity;
        if (check_kind(TokenKind::Text)) {
            from_multiplicity = advance().text;
        }

        if (!check_kind(TokenKind::Arrow)) {
            // 裸类名声明
            ensure(left, left);
            return expect_end_of_statement();
        }
        const std::string arrow_text = advance().text;
        const RelationPattern* relation = match_relation(arrow_text);
        if (relation == nullptr) {
            report(ErrorCode::Unsupported, "无法识别的类图关系箭头");
            skip_to_end_of_line();
            return false;
        }

        std::string to_multiplicity;
        if (check_kind(TokenKind::Text)) {
            to_multiplicity = advance().text;
        }
        if (!(check_kind(TokenKind::Identifier) || check_kind(TokenKind::Text))) {
            report(ErrorCode::ParseError, "关系箭头后缺少目标类名");
            return false;
        }
        const std::string right = advance().text;

        std::string label;
        if (match_punct(":")) {
            const std::size_t start = current().offset;
            std::size_t end = start;
            while (end < source().size() && source()[end] != '\n' && source()[end] != '\r') {
                ++end;
            }
            label = trim(source().substr(start, end - start));
            skip_to_end_of_line();
        } else {
            skip_to_end_of_line();
        }

        ensure(left, left);
        ensure(right, right);
        AstEdge edge;
        edge.from = relation->reversed ? right : left;
        edge.to = relation->reversed ? left : right;
        edge.label = label;
        edge.arrow = relation->at_target ? ArrowKind::AtTarget : ArrowKind::None;
        edge.line_style = std::string(relation->line_style);
        edge.attributes.push_back(AstAttribute{"relation", std::string(relation->relation)});
        // 多重度按"挂在哪个端点"命名：A "1" --> "*" B 里 "1" 属于 A。
        // 关系方向被反转时（A <|-- B），端点也跟着换，键名必须同步换。
        if (!from_multiplicity.empty()) {
            edge.attributes.push_back(AstAttribute{
                relation->reversed ? "to_multiplicity" : "from_multiplicity", from_multiplicity});
        }
        if (!to_multiplicity.empty()) {
            edge.attributes.push_back(AstAttribute{
                relation->reversed ? "from_multiplicity" : "to_multiplicity", to_multiplicity});
        }
        ast_.edges.push_back(std::move(edge));
        return true;
    }

    // 括号内的逐字文本（用于 `class X["显示名"]`）
    std::string capture_bracketed_text(std::string_view open, std::string_view close) {
        if (!match_punct(open)) {
            return {};
        }
        const std::size_t start = current().offset;
        std::size_t end = start;
        while (end < source().size() && source().compare(end, close.size(), close) != 0) {
            ++end;
        }
        if (end >= source().size()) {
            report(ErrorCode::ParseError, "括号未闭合");
            return {};
        }
        std::string text = trim(source().substr(start, end - start));
        // `class X["显示名"]`：引号是语法的一部分，剥掉一层。
        if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
            text = text.substr(1, text.size() - 2);
        }
        const std::size_t after = end + close.size();
        while (current().kind != TokenKind::End && current().offset < after) {
            (void)advance();
        }
        return text;
    }

    AstNode& ensure(const std::string& key, const std::string& label) {
        if (AstNode* existing = ast_.find_node(key)) {
            if (existing->label.empty() || existing->label == existing->key) {
                existing->label = label;
            }
            return *existing;
        }
        if (ast_.nodes.size() >= options_.max_nodes) {
            report(ErrorCode::OutOfRange, "类数超过 max_nodes");
            return ast_.ensure_node(key);
        }
        AstNode& node = ast_.ensure_node(key);
        node.label = label;
        return node;
    }

    ClassParseOptions options_{};
    AstGraph ast_;
};

}  // namespace

Result<AstGraph> parse_class_diagram(std::string_view source, const ClassParseOptions& options,
                                     ParseDiagnostic* out_diagnostic) {
    const Result<std::vector<Token>> tokens = tokenize(source, class_keywords());
    if (!tokens.has_value()) {
        if (out_diagnostic != nullptr) {
            out_diagnostic->code = tokens.error();
            out_diagnostic->message = "词法分析失败";
        }
        return Unexpected<ErrorCode>(tokens.error());
    }
    ClassParser parser(*tokens, source, options);
    Result<AstGraph> result = parser.run();
    if (!result.has_value() && out_diagnostic != nullptr) {
        *out_diagnostic = parser.diagnostic();
    }
    return result;
}

}  // namespace termchart::dsl
