#include "termchart/dsl/d2.hpp"

#include <string>
#include <vector>

namespace termchart::dsl {
namespace {

// D2 的"属性名"集合：出现在 `x: 值` 左侧时按属性处理，否则按形状类型。
constexpr std::string_view kD2Keywords[] = {
    "direction", "label", "shape", "style", "width", "height", "near",
    "tooltip",   "icon",  "link",   "classes", "vars", "constraint",
};

KeywordSet d2_keywords() noexcept {
    return KeywordSet(std::span<const std::string_view>(kD2Keywords));
}

bool is_property(std::string_view text) noexcept {
    for (const std::string_view keyword : kD2Keywords) {
        if (text == keyword) {
            return true;
        }
    }
    return false;
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

// D2 形状名 → 本库的形状词汇。未收录的类型一律按 box，并把原名记进属性。
std::string shape_of(std::string_view type) {
    if (type == "circle") {
        return "circle";
    }
    if (type == "oval" || type == "ellipse") {
        return "round";
    }
    if (type == "diamond" || type == "decision") {
        return "diamond";
    }
    if (type == "hexagon") {
        return "hexagon";
    }
    if (type == "parallelogram" || type == "step" || type == "package" ||
        type == "callout") {
        return "asymmetric";
    }
    return "box";
}

std::string direction_of(std::string_view value) {
    if (value == "right") {
        return "LR";
    }
    if (value == "left") {
        return "RL";
    }
    if (value == "up") {
        return "BT";
    }
    if (value == "down") {
        return "TB";
    }
    return {};
}

class D2Parser : public ParserBase {
public:
    D2Parser(std::vector<Token> tokens, std::string_view source, const D2ParseOptions& options)
        : ParserBase(std::move(tokens), "d2", source), options_(options) {}

    Result<AstGraph> run() {
        parse_block(/*owner=*/std::string{});
        if (failed()) {
            return Unexpected<ErrorCode>(diagnostic().code);
        }
        return ast_;
    }

private:
    // 返回 false 表示失败（failed() 已被置位）。
    void parse_block(const std::string& owner) {
        while (!at_end()) {
            skip_newlines();
            if (at_end()) {
                break;
            }
            if (match_punct(";")) {
                continue;
            }
            if (check_punct("}")) {
                if (owners_.empty()) {
                    report(ErrorCode::ParseError, "多余的 }");
                    (void)advance();
                    continue;
                }
                (void)advance();
                owners_.pop_back();
                return;  // 本块结束
            }
            if (!parse_statement(owner) || failed()) {
                return;
            }
        }
    }

    // `style: { … }`：整块跳过（认识它，但不把它变成图结构）。
    void skip_style_block() {
        if (!match_punct("{")) {
            skip_to_end_of_line();
            return;
        }
        int depth = 1;
        while (!at_end() && depth > 0) {
            const Token token = advance();
            if (token.is_punct("{")) {
                ++depth;
            } else if (token.is_punct("}")) {
                --depth;
            }
        }
    }

    bool parse_statement(const std::string& owner) {
        // `#` 注释默认被词法层丢掉；这里防 keep_comments=true 的情况。
        if (check_kind(TokenKind::Comment)) {
            (void)advance();
            return true;
        }
        // D2 的属性名（label/shape/direction/…）在词法层就是关键字，因此语句头
        // 必须同时接受关键字与标识符——否则 `direction: right` 会在第一步就被拒。
        if (!(check_kind(TokenKind::Identifier) || check_kind(TokenKind::Keyword) ||
              check_kind(TokenKind::Number) || check_kind(TokenKind::Text))) {
            report(ErrorCode::ParseError, "期望 D2 语句（节点名 / 属性 / 连接）");
            return false;
        }
        std::string lhs = advance().text;

        // `style: { ... }` 在任何人名之前处理（style 是关键字而非节点名）。
        if (lhs == "style" && check_punct(":")) {
            (void)advance();  // ':'
            skip_style_block();
            return true;
        }

        // `name.prop: value`。点号可能已被词法层吃进标识符（D2 写 `a.tooltip: 提示`），
        // 因此这里同时处理"加空格的 `a . prop`"与"粘连的 `a.prop`"两种写法；
        // 粘连写法只有在点号后面确实是**已知属性名**时才拆分，否则按节点名处理。
        std::string property;
        if (match_punct(".")) {
            if (!(check_kind(TokenKind::Identifier) || check_kind(TokenKind::Keyword) ||
                  check_kind(TokenKind::Number))) {
                report(ErrorCode::ParseError, "'.' 后缺少属性名");
                return false;
            }
            property = advance().text;
        } else {
            const std::size_t dot = lhs.find('.');
            if (dot != std::string::npos && is_property(lhs.substr(dot + 1))) {
                property = lhs.substr(dot + 1);
                lhs = lhs.substr(0, dot);
            }
        }

        if (!property.empty()) {
            if (!match_punct(":")) {
                report(ErrorCode::ParseError, "属性缺少 ':'");
                return false;
            }
            const std::string value = rest_of_line();
            apply_property(lhs, property, value);
            return true;
        }

        // `a.: b` 这类"点了却没属性"的写法：报错而不是当成名叫 "a." 的节点。
        if (lhs.size() > 1 && lhs.back() == '.') {
            report(ErrorCode::ParseError, "'.' 后缺少属性名");
            return false;
        }

        if (match_punct(":")) {
            if (check_punct("{")) {
                // 容器块：`key: { … }`
                AstNode& node = ensure(lhs);
                node.attributes.push_back(AstAttribute{"container", "true"});
                last_key_ = lhs;
                (void)advance();  // '{'
                owners_.push_back(lhs);
                parse_block(lhs);
                return !failed();
            }
            if (check_kind(TokenKind::Arrow)) {
                return parse_connection(lhs);  // `a: -> b` 之类不合法，交给连接处理
            }
            if (is_property(lhs) && (check_kind(TokenKind::Text) ||
                                     check_kind(TokenKind::Identifier) ||
                                     check_kind(TokenKind::Number))) {
                // 顶层 `label: 文字` —— 作用于当前块（或顶层最近节点）。
                const std::string value = rest_of_line();
                const std::string target = owner.empty() ? last_key_ : owner;
                apply_property(target, lhs, value);
                return true;
            }
            if (check_kind(TokenKind::Identifier) || check_kind(TokenKind::Text) ||
                check_kind(TokenKind::Number)) {
                const std::string type = advance().text;
                AstNode& node = ensure(lhs);
                node.shape = shape_of(type);
                node.attributes.push_back(AstAttribute{"d2_shape", type});
                last_key_ = lhs;
                return true;
            }
            // `key:` 后面没有内容：当节点声明。
            AstNode& node = ensure(lhs);
            (void)node;
            last_key_ = lhs;
            return true;
        }

        if (check_kind(TokenKind::Arrow)) {
            return parse_connection(lhs);
        }

        // 裸节点名。
        (void)ensure(lhs);
        last_key_ = lhs;
        return true;
    }

    bool parse_connection(const std::string& first) {
        std::string from = first;
        while (check_kind(TokenKind::Arrow)) {
            const std::string arrow = advance().text;
            const bool reversed = arrow == "<-";
            if (!(check_kind(TokenKind::Identifier) || check_kind(TokenKind::Text) ||
                  check_kind(TokenKind::Number))) {
                report(ErrorCode::ParseError, "连接箭头后缺少目标节点");
                return false;
            }
            const std::string to = advance().text;
            std::string label;
            if (match_punct(":")) {
                label = rest_of_line();
            }
            (void)ensure(from);
            (void)ensure(to);
            AstEdge edge;
            edge.from = reversed ? to : from;
            edge.to = reversed ? from : to;
            edge.label = std::move(label);
            edge.arrow = arrow == "--" ? ArrowKind::None : ArrowKind::AtTarget;
            ast_.edges.push_back(std::move(edge));
            from = to;
        }
        return true;
    }

    void apply_property(const std::string& key, const std::string& property,
                        const std::string& value) {
        // 方向是**图级**属性：任何层级写它都生效（D2 允许在容器里改方向）。
        if (property == "direction") {
            const std::string direction = direction_of(value);
            if (!direction.empty()) {
                ast_.direction = direction;
            } else {
                ast_.attributes.push_back(AstAttribute{"direction_raw", value});
            }
            return;
        }
        if (key.empty()) {
            // 顶层属性但还没有任何节点：记到图上，不造幽灵节点。
            ast_.attributes.push_back(AstAttribute{property, value});
            return;
        }
        AstNode& node = ensure(key);
        if (property == "label") {
            node.label = value;
        } else if (property == "shape") {
            node.shape = shape_of(value);
            node.attributes.push_back(AstAttribute{"d2_shape", value});
        } else if (property == "direction") {
            ast_.direction = direction_of(value);
        } else {
            node.attributes.push_back(AstAttribute{property, value});
        }
    }

    std::string rest_of_line() {
        const std::size_t start = current().offset;
        std::size_t end = start;
        while (end < source().size() && source()[end] != '\n' && source()[end] != '\r') {
            ++end;
        }
        const std::string text = trim(source().substr(start, end - start));
        skip_to_end_of_line();
        return text;
    }

    AstNode& ensure(const std::string& key) {
        if (AstNode* existing = const_cast<AstNode*>(ast_.find_node(key))) {
            return *existing;
        }
        if (ast_.nodes.size() >= options_.max_nodes) {
            report(ErrorCode::OutOfRange, "节点数超过 max_nodes");
            return ast_.ensure_node(key);
        }
        AstNode& node = ast_.ensure_node(key);
        if (!owners_.empty()) {
            node.attributes.push_back(AstAttribute{"parent", owners_.back()});
        }
        return node;
    }

    D2ParseOptions options_{};
    AstGraph ast_;
    std::string last_key_;
    std::vector<std::string> owners_;
};

}  // namespace

Result<AstGraph> parse_d2(std::string_view source, const D2ParseOptions& options,
                          ParseDiagnostic* out_diagnostic) {
    const Result<std::vector<Token>> tokens = tokenize(source, d2_keywords());
    if (!tokens.has_value()) {
        if (out_diagnostic != nullptr) {
            out_diagnostic->code = tokens.error();
            out_diagnostic->message = "词法分析失败";
        }
        return Unexpected<ErrorCode>(tokens.error());
    }
    D2Parser parser(*tokens, source, options);
    Result<AstGraph> result = parser.run();
    if (!result.has_value() && out_diagnostic != nullptr) {
        *out_diagnostic = parser.diagnostic();
    }
    return result;
}

}  // namespace termchart::dsl
