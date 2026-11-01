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

// D2 里三个块级声明的关键字。它们后面跟的是 `{ … }` 块，**不是**节点。
constexpr std::string_view kD2BlockKeywords[] = {"style", "vars", "classes"};

bool is_block_keyword(std::string_view text) noexcept {
    for (const std::string_view keyword : kD2BlockKeywords) {
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

// D2 的一个块内条目：`key: value` 或 `key: { … }`（嵌套）。
// style / vars 是"属性名 → 值"；classes 是"类名 → 属性集合"（一层嵌套）。
struct D2BlockEntry {
    std::string key;
    std::string value;
    std::vector<D2BlockEntry> children;
};

// 一个类定义（`classes: { foo: { fill: red } }` 里的 foo）。
struct D2ClassDef {
    std::string name;
    std::vector<AstAttribute> properties;  // key = 属性名（"fill"/"stroke"/…）
};

// 去掉值外层的引号（`"#f00"` → `#f00`）。引号不配对时原样返回。
std::string unquote(std::string_view text) {
    if (text.size() >= 2 &&
        (text.front() == '"' || text.front() == '\'') && text.back() == text.front()) {
        return std::string(text.substr(1, text.size() - 2));
    }
    return std::string(text);
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
        // 套用顺序决定优先级：节点自带 style > 类样式 > 全局 style。
        apply_class_styles();
        apply_global_style();
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

    // ── 块级声明（style / vars / classes）────────────────────────────────
    //
    // 历史行为（两处都错，且 classes/vars 更糟）：
    //   * `style: { … }` 的内容被**整块跳过** —— fill / stroke 全丢；
    //   * `classes: { … }` / `vars: { … }` 落进"容器块"分支，凭空造出名为
    //     classes / vars 的**幽灵节点**，甚至把 `vars: { x: 5 }` 里的 x 解析成节点。
    // 现在统一按块解析：内容进图级属性袋 / 节点属性，绝不造节点。

    // 前进到源偏移 >= target 的位置（token 的 offset 单调递增，可直接比较）。
    void advance_to_offset(std::size_t target) {
        while (!at_end() && current().offset < target) {
            (void)advance();
        }
    }

    // 读块内的一个值：到 ';' / '}' / 行尾为止；引号内的 ';' 不算分隔符。
    std::string read_block_value() {
        const std::size_t start = current().offset;
        std::size_t end = start;
        bool in_quotes = false;
        while (end < source().size()) {
            const char ch = source()[end];
            if (ch == '"' || ch == '\'') {
                in_quotes = !in_quotes;
                ++end;
                continue;
            }
            if (!in_quotes && (ch == ';' || ch == '}' || ch == '\n' || ch == '\r')) {
                break;
            }
            ++end;
        }
        const std::string text = trim(source().substr(start, end - start));
        advance_to_offset(end);
        return unquote(text);
    }

    // 解析 `{ … }` 里的键值对（可嵌套一层，供 classes 用）。
    bool parse_block_entries(std::vector<D2BlockEntry>& out) {
        if (!match_punct("{")) {
            report(ErrorCode::ParseError, "期望 '{'");
            return false;
        }
        while (!at_end()) {
            skip_newlines();
            if (at_end()) {
                break;
            }
            if (match_punct(";")) {
                continue;
            }
            if (match_punct("}")) {
                return true;
            }
            if (!(check_kind(TokenKind::Identifier) || check_kind(TokenKind::Keyword) ||
                  check_kind(TokenKind::Number) || check_kind(TokenKind::Text))) {
                report(ErrorCode::ParseError, "块内期望属性名");
                return false;
            }
            D2BlockEntry entry;
            entry.key = advance().text;
            if (!match_punct(":")) {
                report(ErrorCode::ParseError, "块内属性缺少 ':'");
                return false;
            }
            if (check_punct("{")) {
                if (!parse_block_entries(entry.children)) {
                    return false;
                }
            } else {
                const std::size_t before = token_index();
                entry.value = read_block_value();
                if (token_index() == before && !check_punct("}") && !check_punct(";")) {
                    // 值读空且游标没动：强制前进一个 token，保证循环一定推进
                    //（fuzz 输入下"游标停住"就是挂死）。
                    (void)advance();
                }
            }
            out.push_back(std::move(entry));
        }
        report(ErrorCode::ParseError, "块缺少 '}'");
        return false;
    }

    // 处理 `style: { … }` / `vars: { … }` / `classes: { … }`。
    bool parse_block_declaration(const std::string& keyword) {
        std::vector<D2BlockEntry> entries;
        if (!parse_block_entries(entries)) {
            return false;
        }
        if (keyword == "style") {
            for (const D2BlockEntry& entry : entries) {
                global_style_.push_back(AstAttribute{entry.key, entry.value});
                ast_.attributes.push_back(AstAttribute{"style." + entry.key, entry.value});
            }
        } else if (keyword == "vars") {
            for (const D2BlockEntry& entry : entries) {
                variables_.push_back(AstAttribute{entry.key, entry.value});
                ast_.attributes.push_back(AstAttribute{"var." + entry.key, entry.value});
            }
        } else {
            for (const D2BlockEntry& entry : entries) {
                D2ClassDef definition;
                definition.name = entry.key;
                for (const D2BlockEntry& property : entry.children) {
                    definition.properties.push_back(AstAttribute{property.key, property.value});
                    ast_.attributes.push_back(
                        AstAttribute{"class." + entry.key + "." + property.key, property.value});
                }
                classes_.push_back(std::move(definition));
            }
        }
        return true;
    }

    // 节点属性里是否已有某个 key。
    static bool has_attribute(const AstNode& node, std::string_view key) noexcept {
        for (const AstAttribute& attribute : node.attributes) {
            if (attribute.key == key) {
                return true;
            }
        }
        return false;
    }

    static const AstAttribute* find_attribute(const AstNode& node, std::string_view key) noexcept {
        for (const AstAttribute& attribute : node.attributes) {
            if (attribute.key == key) {
                return &attribute;
            }
        }
        return nullptr;
    }

    // D2 语义：`x.classes: foo bar` 里类名以空白或逗号分隔。
    static std::vector<std::string> split_class_names(std::string_view value) {
        std::vector<std::string> names;
        std::size_t begin = 0;
        while (begin <= value.size()) {
            std::size_t end = begin;
            while (end < value.size() && value[end] != ' ' && value[end] != '\t' &&
                   value[end] != ',' && value[end] != '\n' && value[end] != '\r') {
                ++end;
            }
            if (end > begin) {
                names.emplace_back(value.substr(begin, end - begin));
            }
            if (end >= value.size()) {
                break;
            }
            begin = end + 1;
        }
        return names;
    }

    // D2 语义：类样式套用到引用它的节点上；节点自己显式写的 style 优先。
    // 先于全局 style 应用 —— 否则全局 style 会先把 key 占住，类样式就再也套不上。
    void apply_class_styles() {
        if (classes_.empty()) {
            return;
        }
        for (AstNode& node : ast_.nodes) {
            const AstAttribute* reference = find_attribute(node, "classes");
            if (reference == nullptr) {
                continue;
            }
            for (const std::string& name : split_class_names(reference->value)) {
                for (const D2ClassDef& definition : classes_) {
                    if (definition.name != name) {
                        continue;
                    }
                    for (const AstAttribute& property : definition.properties) {
                        const std::string key = "style." + property.key;
                        if (!has_attribute(node, key)) {
                            node.attributes.push_back(AstAttribute{key, property.value});
                        }
                    }
                }
            }
        }
    }

    // D2 语义：顶层 `style: { … }` 对**所有**节点生效（与声明顺序无关）。
    void apply_global_style() {
        if (global_style_.empty()) {
            return;
        }
        for (AstNode& node : ast_.nodes) {
            for (const AstAttribute& property : global_style_) {
                const std::string key = "style." + property.key;
                if (!has_attribute(node, key)) {
                    node.attributes.push_back(AstAttribute{key, property.value});
                }
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

        // D2 的三个块级声明：`style: { … }` / `vars: { … }` / `classes: { … }`。
        // 它们是**关键字**而非节点名，后面跟的是块。在任何人名之前处理。
        if (is_block_keyword(lhs) && check_punct(":")) {
            (void)advance();  // ':'
            return parse_block_declaration(lhs);
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
            if (property == "style" && check_punct("{")) {
                // `x.style: { … }`：节点级样式块。逐条落成 "style.<属性>" 属性，
                // 不再把整块原文塞进一个字符串。
                if (lhs.empty()) {
                    report(ErrorCode::ParseError, "style 块缺少宿主节点");
                    return false;
                }
                std::vector<D2BlockEntry> entries;
                if (!parse_block_entries(entries)) {
                    return false;
                }
                AstNode& node = ensure(lhs);
                for (const D2BlockEntry& entry : entries) {
                    node.attributes.push_back(AstAttribute{"style." + entry.key, entry.value});
                }
                return true;
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
        if (AstNode* existing = ast_.find_node(key)) {
            return *existing;
        }
        if (ast_.nodes.size() >= options_.max_nodes) {
            // 超限只报错，**不再**造节点：历史上这里报错之后仍然 ensure_node()，
            // 于是"超限时到底建没建"成了薛定谔状态——run() 最后固然因 failed() 丢弃
            // 结果，但过程中的 nodes.size() 已经越过上限，任何边界断言都形同虚设。
            report(ErrorCode::OutOfRange, "节点数超过 max_nodes");
            overflow_node_ = AstNode{};
            overflow_node_.key.assign(key);
            return overflow_node_;  // 调用方拿到的是"黑洞"，不进 ast_.nodes
        }
        AstNode& node = ast_.ensure_node(key);
        if (!owners_.empty()) {
            node.attributes.push_back(AstAttribute{"parent", owners_.back()});
        }
        return node;
    }

    D2ParseOptions options_{};
    AstGraph ast_;
    // 超限后的返回值落点：不属于 ast_.nodes，仅供调用方继续走完语法（结果会被丢弃）。
    AstNode overflow_node_;
    std::string last_key_;
    std::vector<std::string> owners_;
    // 块级声明的解析结果（解析结束后统一套用到节点上）。
    std::vector<AstAttribute> global_style_;  // 顶层 style
    std::vector<AstAttribute> variables_;     // vars（目前只解析、不做 ${} 替换）
    std::vector<D2ClassDef> classes_;         // classes
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
