#include "termchart/dsl/plantuml.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace termchart::dsl {
namespace {

constexpr std::size_t kMaxEdges = 200000;

constexpr std::string_view kPlantUmlKeywords[] = {
    "startuml", "enduml", "class", "abstract", "interface", "enum",
};

KeywordSet plantuml_keywords() noexcept {
    return KeywordSet(std::span<const std::string_view>(kPlantUmlKeywords));
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

std::string strip_quotes(std::string value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

// 关系操作符的语义。swap=true 表示"标记在左端"（`<|--` 之类），需要交换 from/to。
struct RelationInfo {
    bool matched = false;
    const char* relation = "";
    ArrowKind arrow = ArrowKind::AtTarget;
    const char* line_style = "";
    bool swap = false;
};

RelationInfo classify_relation(std::string_view op) {
    if (op == "-->") {
        return RelationInfo{true, "association", ArrowKind::AtTarget, "", false};
    }
    if (op == "--") {
        return RelationInfo{true, "association", ArrowKind::None, "", false};
    }
    if (op == "--|>") {
        return RelationInfo{true, "inheritance", ArrowKind::AtTarget, "", false};
    }
    if (op == "<|--") {
        return RelationInfo{true, "inheritance", ArrowKind::AtTarget, "", true};
    }
    if (op == "..|>") {
        return RelationInfo{true, "inheritance", ArrowKind::AtTarget, "dashed", false};
    }
    if (op == "<|..") {
        return RelationInfo{true, "inheritance", ArrowKind::AtTarget, "dashed", true};
    }
    if (op == "..>") {
        return RelationInfo{true, "dependency", ArrowKind::AtTarget, "dashed", false};
    }
    if (op == "*--") {
        return RelationInfo{true, "composition", ArrowKind::None, "", false};
    }
    if (op == "o--") {
        return RelationInfo{true, "aggregation", ArrowKind::None, "", false};
    }
    if (op == "--*") {
        return RelationInfo{true, "composition", ArrowKind::None, "", true};
    }
    if (op == "--o") {
        return RelationInfo{true, "aggregation", ArrowKind::None, "", true};
    }
    return RelationInfo{};
}

class PlantUmlParser : public ParserBase {
public:
    PlantUmlParser(std::vector<Token> tokens, std::string_view source,
                   const PlantUmlParseOptions& options)
        : ParserBase(std::move(tokens), "plantuml", source), options_(options) {}

    Result<AstGraph> run() {
        parse_preamble();
        if (!failed()) {
            parse_body();
        }
        if (failed()) {
            return Unexpected<ErrorCode>(diagnostic().code);
        }
        return ast_;
    }

private:
    // ── 头部：@startuml [名字] ─────────────────────────────────────────
    void parse_preamble() {
        skip_newlines();
        if (!check_punct("@")) {
            report(ErrorCode::ParseError, "缺少 @startuml 头部");
            return;
        }
        (void)advance();  // '@'
        if (!match_keyword("startuml")) {
            report(ErrorCode::ParseError, "缺少 @startuml 头部");
            return;
        }
        ast_.directive = "@startuml";
        if (check_kind(TokenKind::Identifier) || check_kind(TokenKind::Text) ||
            check_kind(TokenKind::Number)) {
            ast_.name = advance().text;
        }
        skip_to_end_of_line();
    }

    // ── 顶层循环 ───────────────────────────────────────────────────────
    void parse_body() {
        while (true) {
            skip_newlines();
            if (at_end()) {
                break;
            }
            if (check_punct("@")) {
                (void)advance();
                if (check_keyword("enduml")) {
                    (void)advance();
                    saw_end_ = true;
                    skip_to_end_of_line();
                    break;
                }
                skip_to_end_of_line();  // 其它 @ 指令：忽略
                continue;
            }
            if (check_keyword("class") || check_keyword("abstract") ||
                check_keyword("interface") || check_keyword("enum")) {
                parse_class();
            } else if (is_note_start()) {
                skip_note();
            } else if (is_relation_start()) {
                parse_relation();
            } else {
                skip_to_end_of_line();  // 未识别：整行忽略，绝不改变已解析结构
                continue;
            }
            if (failed()) {
                return;
            }
        }
        if (!saw_end_) {
            report(ErrorCode::ParseError, "缺少 @enduml 结尾");
        }
    }

    bool is_note_start() const {
        return check_kind(TokenKind::Identifier) && iequals(current().text, "note");
    }

    // note：单行（有 `:`）快进到行尾；块状（无 `:`）跳到 `end` 行。
    void skip_note() {
        bool single_line = false;
        while (!at_end() && current().kind != TokenKind::Newline) {
            if (check_punct(":")) {
                single_line = true;
            }
            (void)advance();
        }
        if (single_line) {
            return;
        }
        while (!at_end()) {
            skip_newlines();
            if (at_end()) {
                return;
            }
            if (check_punct("@")) {
                return;  // 交给外层处理（可能是 @enduml）
            }
            if (check_kind(TokenKind::Identifier) && iequals(current().text, "end")) {
                skip_to_end_of_line();
                return;
            }
            skip_to_end_of_line();
        }
    }

    // ── 类声明：class 名称 [as 别名] [{ … }] ──────────────────────────
    void parse_class() {
        if (check_keyword("abstract")) {
            (void)advance();
            if (!match_keyword("class")) {
                skip_to_end_of_line();
                return;
            }
        } else {
            (void)advance();  // class / interface / enum
        }
        if (!check_kind(TokenKind::Identifier) && !check_kind(TokenKind::Text)) {
            skip_to_end_of_line();
            return;
        }
        const std::string name = advance().text;
        std::string key = name;
        if (check_kind(TokenKind::Identifier) && iequals(current().text, "as")) {
            (void)advance();
            if (check_kind(TokenKind::Identifier) || check_kind(TokenKind::Text)) {
                key = advance().text;
            }
        }
        AstNode* node = ensure_node(key);
        if (node == nullptr) {
            return;
        }
        node->label = name;
        if (check_punct("{")) {
            (void)advance();
            parse_class_body(*node);
            if (failed()) {
                return;
            }
        }
        skip_to_end_of_line();
    }

    void parse_class_body(AstNode& node) {
        std::size_t depth = 1;
        while (!at_end()) {
            if (check_punct("{")) {
                (void)advance();
                ++depth;
                continue;
            }
            if (check_punct("}")) {
                (void)advance();
                --depth;
                if (depth == 0) {
                    return;
                }
                continue;
            }
            if (current().kind == TokenKind::Newline) {
                (void)advance();
                continue;
            }
            // 收集本行原文（到行尾或下一个花括号之前）作为一条成员。
            const std::size_t start = current().offset;
            std::size_t end = start;
            while (!at_end() && current().kind != TokenKind::Newline && !check_punct("}") &&
                   !check_punct("{")) {
                const Token token = advance();
                end = token.offset + token.length;
            }
            const std::string member = trim(source().substr(start, end - start));
            if (!member.empty()) {
                node.attributes.push_back(AstAttribute{"member", member});
            }
        }
        report(ErrorCode::ParseError, "类体未闭合");
    }

    // ── 关系：from [mult] OP [mult] to [: label] ──────────────────────
    bool is_relation_start() const {
        if (!check_kind(TokenKind::Identifier) && !check_kind(TokenKind::Text) &&
            !check_kind(TokenKind::Number)) {
            return false;
        }
        std::size_t ahead = 1;
        if (peek(1).kind == TokenKind::Text) {
            ahead = 2;  // 源端多重度
        }
        if (peek(ahead).kind != TokenKind::Arrow) {
            return false;
        }
        return classify_relation(peek(ahead).text).matched;
    }

    void parse_relation() {
        const std::string left_id = advance().text;
        std::string left_mult;
        if (current().kind == TokenKind::Text) {
            left_mult = advance().text;
        }
        const Token op_token = advance();  // Arrow
        const RelationInfo rel = classify_relation(op_token.text);
        if (!rel.matched) {
            skip_to_end_of_line();
            return;
        }
        std::string right_mult;
        if (current().kind == TokenKind::Text) {
            right_mult = advance().text;
        }
        if (!check_kind(TokenKind::Identifier) && !check_kind(TokenKind::Text) &&
            !check_kind(TokenKind::Number)) {
            report(ErrorCode::ParseError, "关系缺少目标类");
            return;
        }
        const std::string right_id = advance().text;

        if (ensure_node(left_id) == nullptr || ensure_node(right_id) == nullptr) {
            return;
        }
        if (ast_.edges.size() >= kMaxEdges) {
            report(ErrorCode::OutOfRange, "边数超过上限");
            return;
        }

        AstEdge edge;
        if (rel.swap) {
            edge.from = right_id;
            edge.to = left_id;
        } else {
            edge.from = left_id;
            edge.to = right_id;
        }
        edge.arrow = rel.arrow;
        edge.line_style = rel.line_style;
        edge.attributes.push_back(AstAttribute{"relation", rel.relation});
        const std::string& from_mult = rel.swap ? right_mult : left_mult;
        const std::string& to_mult = rel.swap ? left_mult : right_mult;
        if (!from_mult.empty()) {
            edge.attributes.push_back(AstAttribute{"from_multiplicity", from_mult});
        }
        if (!to_mult.empty()) {
            edge.attributes.push_back(AstAttribute{"to_multiplicity", to_mult});
        }
        if (check_punct(":")) {
            (void)advance();
            edge.label = capture_line_tail();
        }
        ast_.edges.push_back(std::move(edge));
        skip_to_end_of_line();
    }

    // 当前 token 起到行尾的原文（去首尾空白与成对引号）。
    std::string capture_line_tail() const {
        if (at_end() || current().kind == TokenKind::Newline) {
            return {};
        }
        const std::string_view src = source();
        const std::size_t start = current().offset;
        std::size_t cursor = start;
        while (cursor < src.size() && src[cursor] != '\n' && src[cursor] != '\r') {
            ++cursor;
        }
        return strip_quotes(trim(src.substr(start, cursor - start)));
    }

    AstNode* ensure_node(const std::string& key) {
        if (AstNode* existing = const_cast<AstNode*>(ast_.find_node(key))) {
            return existing;
        }
        if (ast_.nodes.size() >= options_.max_nodes) {
            report(ErrorCode::OutOfRange, "节点数超过 max_nodes");
            return nullptr;
        }
        return &ast_.ensure_node(key);
    }

    PlantUmlParseOptions options_{};
    AstGraph ast_;
    bool saw_end_ = false;
};

}  // namespace

Result<AstGraph> parse_plantuml(std::string_view source, const PlantUmlParseOptions& options,
                                ParseDiagnostic* out_diagnostic) {
    LexerOptions lexer_options;
    lexer_options.comment_prefixes = {"'", "/'"};
    const Result<std::vector<Token>> tokens = tokenize(source, plantuml_keywords(), lexer_options);
    if (!tokens.has_value()) {
        if (out_diagnostic != nullptr) {
            out_diagnostic->code = tokens.error();
            out_diagnostic->message = "词法分析失败";
        }
        return Unexpected<ErrorCode>(tokens.error());
    }
    PlantUmlParser parser(*tokens, source, options);
    Result<AstGraph> result = parser.run();
    if (!result.has_value() && out_diagnostic != nullptr) {
        *out_diagnostic = parser.diagnostic();
    }
    return result;
}

}  // namespace termchart::dsl
