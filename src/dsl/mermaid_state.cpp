#include "termchart/dsl/mermaid_state.hpp"

#include <string>
#include <vector>

namespace termchart::dsl {
namespace {

// `note right of X : …` 里的 right/of 必须是**关键字**：否则它们会被当成状态名，
// 于是整行被拆错（这类 bug 的表现是"报错位置在冒号上"，见测试里的定位断言）。
constexpr std::string_view kStateKeywords[] = {
    "statediagram", "statediagram-v2", "state",  "note",  "direction", "end",  "classdef",
    "class",        "style",           "click",  "as",    "left",      "right", "over",
    "of",
};

KeywordSet state_keywords() noexcept {
    return KeywordSet(std::span<const std::string_view>(kStateKeywords));
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

bool is_pseudo_state(std::string_view key) noexcept { return key == "[*]"; }

class StateParser : public ParserBase {
public:
    StateParser(std::vector<Token> tokens, std::string_view source,
                const StateParseOptions& options)
        : ParserBase(std::move(tokens), "mermaid-state", source), options_(options) {}

    Result<AstGraph> run() {
        parse_header();
        parse_statements(/*expect_end=*/false);
        if (failed()) {
            return Unexpected<ErrorCode>(diagnostic().code);
        }
        return ast_;
    }

private:
    void parse_header() {
        skip_newlines();
        if (!(check_keyword("statediagram") || check_keyword("statediagram-v2"))) {
            report(ErrorCode::ParseError, "缺少 stateDiagram 头部");
            return;
        }
        ast_.directive = advance().text;
    }

    void parse_statements(bool expect_end) {
        while (!at_end()) {
            skip_newlines();
            if (at_end()) {
                break;
            }
            if (match_punct(";")) {
                continue;
            }
            if (check_keyword("end")) {
                if (!expect_end) {
                    report(ErrorCode::ParseError, "多余的 end");
                    (void)advance();
                    continue;
                }
                (void)advance();
                return;
            }
            // 组合态可以用 `}` 收尾（mermaid 两种写法都吃）。
            if (check_punct("}")) {
                if (!expect_end) {
                    report(ErrorCode::ParseError, "多余的 }");
                    (void)advance();
                    continue;
                }
                return;  // 交给 parse_state_block 消费
            }
            if (check_keyword("direction")) {
                (void)advance();
                if (check_kind(TokenKind::Identifier)) {
                    ast_.direction = advance().text;
                }
                skip_to_end_of_line();
                continue;
            }
            if (check_keyword("note")) {
                parse_note();
                continue;
            }
            if (check_keyword("classdef") || check_keyword("class") || check_keyword("style") ||
                check_keyword("click")) {
                skip_to_end_of_line();
                continue;
            }
            if (check_keyword("state")) {
                parse_state_block();
                if (failed()) {
                    return;
                }
                continue;
            }
            if (!parse_transition()) {
                if (failed()) {
                    return;
                }
                skip_to_end_of_line();
            }
        }
        if (expect_end) {
            report(ErrorCode::ParseError, "组合态缺少 end");
        }
    }

    // `state X { … }` / `state X as 显示名` / `state "显示名" as X`
    void parse_state_block() {
        (void)advance();  // state
        std::string key;
        std::string label;
        if (check_kind(TokenKind::Text)) {
            label = advance().text;
            if (check_keyword("as") && check_kind(TokenKind::Identifier)) {
                key = advance().text;
            } else {
                key = label;  // 没有 as：显示名即键
            }
        } else if (check_kind(TokenKind::Identifier)) {
            key = advance().text;
            label = key;
            if (check_keyword("as")) {
                (void)advance();
                if (check_kind(TokenKind::Identifier) || check_kind(TokenKind::Text)) {
                    label = advance().text;
                }
            }
        } else {
            report(ErrorCode::ParseError, "state 后缺少状态名");
            skip_to_end_of_line();
            return;
        }

        AstNode& node = ensure(key, label);
        node.attributes.push_back(AstAttribute{"composite", "true"});

        if (!match_punct("{")) {
            skip_to_end_of_line();
            return;
        }
        const std::string parent = key;
        parents_.push_back(parent);
        const std::size_t body_start = token_index();
        parse_statements(/*expect_end=*/true);
        if (failed()) {
            return;
        }
        // 两种闭合写法：`}`（本次已消费）或 `end` 关键字（已由循环消费）。
        if (token_index() == body_start) {
            report(ErrorCode::ParseError, "组合态为空");
        }
        (void)match_punct("}");
        if (!parents_.empty() && parents_.back() == parent) {
            parents_.pop_back();
        }
    }

    void parse_note() {
        // note left of X : 文本 / note right of X : 文本 / note over X : 文本
        (void)advance();  // note
        std::string placement;
        if (check_keyword("left") || check_keyword("right") || check_keyword("over")) {
            placement = advance().text;  // left / right / over（都在关键字表里）
        }
        std::string target;
        if (match_keyword("of")) {
            if (check_kind(TokenKind::Identifier)) {
                target = advance().text;
            }
        } else if (check_kind(TokenKind::Identifier)) {
            target = advance().text;  // over X（over 已消费）
        }
        // `: 文本` 用原文切片，避免把标点当结构。
        std::string text;
        if (match_punct(":")) {
            text = take_rest_of_line();
        }
        if (target.empty()) {
            return;  // note 目标不认识：忽略（已在头文件声明）
        }
        if (text.empty()) {
            return;
        }
        AstNode& node = ensure(target, target);
        if (placement.empty()) {
            placement = "over";
        }
        node.attributes.push_back(AstAttribute{"note", placement + ": " + text});
    }

    // `A --> B : label` / `[*] --> A`
    bool parse_transition() {
        std::string from;
        bool from_initial = false;
        if (!parse_endpoint(from, from_initial)) {
            return false;
        }
        if (!check_kind(TokenKind::Arrow)) {
            // 裸状态声明：`空闲` 单独一行
            if (!from.empty() && !from_initial) {
                (void)ensure(from, from);
                return true;
            }
            report(ErrorCode::ParseError, "期望 --> 迁移箭头");
            return false;
        }
        while (check_kind(TokenKind::Arrow)) {
            (void)advance();
            std::string to;
            bool to_final = false;
            if (!parse_endpoint(to, to_final)) {
                return false;
            }
            std::string label;
            if (match_punct(":")) {
                label = take_rest_of_line();
            }

            // 伪状态不建节点，而是把语义记到真实状态上。
            if (from_initial && !to_final && !to.empty()) {
                AstNode& node = ensure(to, to);
                node.attributes.push_back(AstAttribute{"initial", "true"});
            } else if (to_final && !from_initial && !from.empty()) {
                AstNode& node = ensure(from, from);
                node.attributes.push_back(AstAttribute{"final", "true"});
            } else if (!from_initial && !to_final) {
                // 边的两端都要登记为状态：只建边不建节点会让图里出现"幽灵状态"。
                (void)ensure(from, from);
                (void)ensure(to, to);
                AstEdge edge;
                edge.from = from;
                edge.to = to;
                edge.label = label;
                edge.arrow = ArrowKind::AtTarget;
                ast_.edges.push_back(std::move(edge));
            } else {
                report(ErrorCode::Unsupported, "[*] --> [*] 不是合法迁移");
                skip_to_end_of_line();
                return false;
            }
            if (!label.empty()) {
                return expect_end_of_statement();
            }
            from = to;
            from_initial = to_final;
        }
        return expect_end_of_statement();
    }

    bool parse_endpoint(std::string& key, bool& pseudo) {
        if (check_punct("[")) {
            // [*]
            const std::size_t start = current().offset;
            if (!match_punct("[")) {
                return false;
            }
            if (!match_punct("*")) {
                report(ErrorCode::Unsupported, "只支持 [*] 伪状态");
                skip_to_end_of_line();
                return false;
            }
            if (!match_punct("]")) {
                report_at(previous_token(), ErrorCode::ParseError, "[*] 缺少闭合 ]");
                return false;
            }
            key = std::string(source().substr(start, 3));
            pseudo = true;
            return true;
        }
        if (check_kind(TokenKind::Identifier) || check_kind(TokenKind::Text) ||
            check_kind(TokenKind::Number)) {
            key = advance().text;
            pseudo = is_pseudo_state(key);
            return true;
        }
        report(ErrorCode::ParseError, "期望状态名");
        return false;
    }

    // `: 文本` 的文本按**原文切片**取（可含标点/中文），然后跳到行尾。
    // 注意不能用 previous_token()：skip_to_end_of_line 直接推 index_，不更新它。
    std::string take_rest_of_line() {
        const std::size_t start = current().offset;
        std::size_t end = start;
        while (end < source().size() && source()[end] != '\n' && source()[end] != '\r') {
            ++end;
        }
        const std::string text = trim(source().substr(start, end - start));
        skip_to_end_of_line();
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
            report(ErrorCode::OutOfRange, "状态数超过 max_nodes");
            // 报错后仍返回一个占位引用，避免调用方空指针；failed() 已被置位。
            return ast_.ensure_node(key);
        }
        AstNode& node = ast_.ensure_node(key);
        node.label = label;
        if (!parents_.empty()) {
            node.attributes.push_back(AstAttribute{"parent", parents_.back()});
        }
        return node;
    }

    StateParseOptions options_{};
    AstGraph ast_;
    std::vector<std::string> parents_;
};

}  // namespace

Result<AstGraph> parse_state_diagram(std::string_view source, const StateParseOptions& options,
                                     ParseDiagnostic* out_diagnostic) {
    const Result<std::vector<Token>> tokens = tokenize(source, state_keywords());
    if (!tokens.has_value()) {
        if (out_diagnostic != nullptr) {
            out_diagnostic->code = tokens.error();
            out_diagnostic->message = "词法分析失败";
        }
        return Unexpected<ErrorCode>(tokens.error());
    }
    StateParser parser(*tokens, source, options);
    Result<AstGraph> result = parser.run();
    if (!result.has_value() && out_diagnostic != nullptr) {
        *out_diagnostic = parser.diagnostic();
    }
    return result;
}

}  // namespace termchart::dsl
