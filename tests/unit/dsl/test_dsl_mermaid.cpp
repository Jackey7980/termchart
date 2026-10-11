// test_dsl_mermaid.cpp —— dsl 模块：lexer / flowchart 解析 / AST→Graph / 注册表
//
// 关注的不是"能不能解析一句话"，而是**契约**：
//   * 非法输入不抛异常、不死循环、错误定位准确；
//   * 支持范围之外的东西要么报错、要么明确忽略，绝不静默错解；
//   * 注册表遵循 function-local static + 快照语义（与另四个注册表同构）。
#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/dsl_registry.hpp"
#include "termchart/dsl/lexer.hpp"
#include "termchart/dsl/mermaid.hpp"
#include "termchart/dsl/mermaid_flow.hpp"
#include "termchart/graph/graph.hpp"

namespace {

using namespace termchart;
using namespace termchart::dsl;

constexpr std::string_view kKeywords[] = {"flowchart", "graph", "subgraph", "end"};

KeywordSet flow_keywords() {
    return KeywordSet(std::span<const std::string_view>(kKeywords, 4));
}

Result<std::vector<Token>> lex(std::string_view source, LexerOptions options = {}) {
    return tokenize(source, flow_keywords(), std::move(options));
}

std::string render_kinds(const std::vector<Token>& tokens) {
    std::string out;
    for (const Token& token : tokens) {
        if (token.kind == TokenKind::End) {
            break;
        }
        switch (token.kind) {
            case TokenKind::Identifier:
                out += "I";
                break;
            case TokenKind::Keyword:
                out += "K";
                break;
            case TokenKind::Number:
                out += "N";
                break;
            case TokenKind::Text:
                out += "S";
                break;
            case TokenKind::Arrow:
                out += "A";
                break;
            case TokenKind::Punctuation:
                out += "P";
                break;
            case TokenKind::Newline:
                out += "E";
                break;
            case TokenKind::Comment:
                out += "C";
                break;
            case TokenKind::Invalid:
                out += "X";
                break;
            case TokenKind::End:
                break;
        }
        out += " ";
    }
    return out;
}

TEST_CASE("dsl/lexer: 箭头与紧邻标识符正确切分") {
    // 老问题：'-' 也是标识符合法字符，处理不好会把 `A-->B` 切成 `A--` / `>` / `B`。
    const auto tokens = lex("A-->B");
    REQUIRE(tokens.has_value());
    CHECK(render_kinds(*tokens) == "I A I ");
    CHECK((*tokens)[0].text == "A");
    CHECK((*tokens)[1].text == "-->");
    CHECK((*tokens)[2].text == "B");
}

TEST_CASE("dsl/lexer: 全部箭头形态各自成 token") {
    const auto tokens = lex("a --- b -.-> c ==> d <--> e --x f");
    REQUIRE(tokens.has_value());
    std::vector<std::string> arrows;
    for (const Token& token : *tokens) {
        if (token.kind == TokenKind::Arrow) {
            arrows.push_back(token.text);
        }
    }
    REQUIRE(arrows.size() == 5);
    CHECK(arrows[0] == "---");
    CHECK(arrows[1] == "-.->");
    CHECK(arrows[2] == "==>");
    CHECK(arrows[3] == "<-->");
    CHECK(arrows[4] == "--x");
}

TEST_CASE("dsl/lexer: 字符串去引号并还原转义") {
    const auto tokens = lex("A[\"say \\\"hi\\\"\"]");
    REQUIRE(tokens.has_value());
    bool found = false;
    for (const Token& token : *tokens) {
        if (token.kind == TokenKind::Text) {
            CHECK(token.text == "say \"hi\"");
            found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("dsl/lexer: 未闭合字符串是 Invalid 而非崩溃") {
    const auto tokens = lex("A[\"unterminated\nB");
    REQUIRE(tokens.has_value());
    bool invalid = false;
    for (const Token& token : *tokens) {
        if (token.kind == TokenKind::Invalid) {
            invalid = true;
            CHECK(token.line == 1);
        }
    }
    CHECK(invalid);
}

TEST_CASE("dsl/lexer: 注释默认丢弃、按需保留") {
    const auto dropped = lex("%% 注释\nA");
    REQUIRE(dropped.has_value());
    for (const Token& token : *dropped) {
        CHECK(token.kind != TokenKind::Comment);
    }

    LexerOptions keep;
    keep.keep_comments = true;
    const auto kept = lex("%% 注释\nA", keep);
    REQUIRE(kept.has_value());
    bool comment_seen = false;
    for (const Token& token : *kept) {
        if (token.kind == TokenKind::Comment) {
            comment_seen = true;
            CHECK(token.text == "%% 注释");
        }
    }
    CHECK(comment_seen);
}

TEST_CASE("dsl/lexer: 非法字节不阻塞游标（防 fuzz 挂死）") {
    const std::string source = std::string("A") + "\x80\xFF" + "B";
    const auto tokens = lex(source);
    REQUIRE(tokens.has_value());
    std::size_t invalid = 0;
    for (const Token& token : *tokens) {
        if (token.kind == TokenKind::Invalid) {
            ++invalid;
            CHECK(token.length >= 1);
        }
    }
    CHECK(invalid >= 1);
    // 依然能看到结尾的 B
    CHECK((*tokens)[tokens->size() - 2].text.find('B') != std::string::npos);
}

TEST_CASE("dsl/mermaid: 识别各类图种首行") {
    CHECK(detect_diagram("flowchart TD\n A-->B") == MermaidDiagram::Flowchart);
    CHECK(detect_diagram("  graph LR\n A-->B") == MermaidDiagram::Flowchart);
    CHECK(detect_diagram("%% 注释\n%% 又一波\nsequenceDiagram\n A->>B") ==
          MermaidDiagram::Sequence);
    CHECK(detect_diagram("classDiagram\n class A") == MermaidDiagram::Class);
    CHECK(detect_diagram("stateDiagram-v2\n [*] --> A") == MermaidDiagram::State);
    CHECK(detect_diagram("") == MermaidDiagram::Unknown);
    CHECK(detect_diagram("digraph G {}") == MermaidDiagram::Unknown);

    SUBCASE("未实现的图种明确报 NotImplemented，而不是假装成功") {
        const Result<AstGraph> result = parse_mermaid("sequenceDiagram\n A->>B");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::NotImplemented);
    }
    SUBCASE("识别不了报 ParseError") {
        const Result<AstGraph> result = parse_mermaid("随便写点什么");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::ParseError);
    }
    SUBCASE("方言名可读") {
        CHECK(std::string(diagram_name(MermaidDiagram::Flowchart)) == "flowchart");
    }
}

TEST_CASE("dsl/flowchart: 节点形状与标签") {
    const std::string source = R"(flowchart TD
        A[开始] --> B(处理)
        B --> C([结束])
        B --> D{判断}
        D --> E((圆))
        D --> F{{六边}}
        F --> G>不对称]
    )";
    const Result<AstGraph> result = parse_flowchart(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;
    CHECK(ast.directive == "flowchart");
    CHECK(ast.direction == "TD");
    REQUIRE(ast.nodes.size() == 7);

    const AstNode* begin = ast.find_node("A");
    REQUIRE(begin != nullptr);
    CHECK(begin->label == "开始");
    CHECK(begin->shape == "box");

    CHECK(ast.find_node("C")->shape == "stadium");
    CHECK(ast.find_node("D")->shape == "diamond");
    CHECK(ast.find_node("E")->shape == "circle");
    CHECK(ast.find_node("F")->shape == "hexagon");
    CHECK(ast.find_node("G")->shape == "asymmetric");
    CHECK(ast.find_node("B")->shape == "round");
}

TEST_CASE("dsl/flowchart: 边的箭头/标签/线型") {
    const std::string source = R"(graph LR
        A --> B
        B --- C
        C -.-> D
        D ==> E
        E <--> F
        F -->|是| G
        G -- 否 --> H
    )";
    const Result<AstGraph> result = parse_flowchart(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;
    REQUIRE(ast.edges.size() == 7);
    CHECK(ast.edges[0].arrow == ArrowKind::AtTarget);
    CHECK(ast.edges[1].arrow == ArrowKind::None);
    CHECK(ast.edges[2].line_style == "dashed");
    CHECK(ast.edges[3].line_style == "thick");
    CHECK(ast.edges[4].arrow == ArrowKind::Both);
    CHECK(ast.edges[5].label == "是");
    CHECK(ast.edges[6].label == "否");
    CHECK(ast.edges[6].arrow == ArrowKind::AtTarget);

    SUBCASE("边链 A --> B --> C 展开为两条边") {
        const Result<AstGraph> chain = parse_flowchart("flowchart TD\n A --> B --> C");
        REQUIRE(chain.has_value());
        REQUIRE(chain->edges.size() == 2);
        CHECK(chain->edges[0].from == "A");
        CHECK(chain->edges[0].to == "B");
        CHECK(chain->edges[1].from == "B");
        CHECK(chain->edges[1].to == "C");
    }
}

TEST_CASE("dsl/flowchart: 先引用后声明时标签以声明为准") {
    const std::string source = "flowchart TD\n A --> B\n B[真正的标签]\n";
    const Result<AstGraph> result = parse_flowchart(source);
    REQUIRE(result.has_value());
    const AstNode* node = result->find_node("B");
    REQUIRE(node != nullptr);
    CHECK(node->label == "真正的标签");
    CHECK(node->shape == "box");
}

TEST_CASE("dsl/flowchart: 子图成员记在 group 属性上") {
    const std::string source = R"(flowchart TD
        subgraph 处理阶段
            A[读取] --> B[校验]
        end
        B --> C[写出]
    )";
    const Result<AstGraph> result = parse_flowchart(source);
    REQUIRE(result.has_value());
    const AstNode* inside = result->find_node("A");
    REQUIRE(inside != nullptr);
    REQUIRE(inside->attributes.size() == 1);
    CHECK(inside->attributes[0].key == "group");
    CHECK(inside->attributes[0].value == "处理阶段");
    CHECK(result->find_node("C")->attributes.empty());

    SUBCASE("严格模式下 subgraph 不被接受") {
        FlowchartParseOptions strict;
        strict.allow_subgraph = false;
        const Result<AstGraph> rejected = parse_flowchart(source, strict);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error() == ErrorCode::Unsupported);
    }
}

TEST_CASE("dsl/flowchart: 认得出但本切片不做的指令被跳过而非报错") {
    const std::string source = R"(flowchart TD
        classDef big fill:#f00
        A[甲] --> B[乙]
        class A big
        click A "https://example.com"
        style B fill:#0f0
    )";
    const Result<AstGraph> result = parse_flowchart(source);
    REQUIRE(result.has_value());
    CHECK(result->nodes.size() == 2);
    CHECK(result->edges.size() == 1);
}

TEST_CASE("dsl/flowchart: 错误路径给出定位且不抛异常") {
    SUBCASE("缺头部") {
        ParseDiagnostic diagnostic;
        const Result<AstGraph> result = parse_flowchart("A --> B", {}, &diagnostic);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::ParseError);
        CHECK(diagnostic.line == 1);
        CHECK_FALSE(diagnostic.message.empty());
    }
    SUBCASE("括号未闭合") {
        const Result<AstGraph> result = parse_flowchart("flowchart TD\n A[没闭合 --> B");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::ParseError);
    }
    SUBCASE("多余的 end") {
        const Result<AstGraph> result = parse_flowchart("flowchart TD\n A --> B\n end\n");
        REQUIRE_FALSE(result.has_value());
    }
    SUBCASE("节点数上限") {
        FlowchartParseOptions options;
        options.max_nodes = 2;
        const Result<AstGraph> result =
            parse_flowchart("flowchart TD\n A --> B\n B --> C\n C --> D", options);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::OutOfRange);
    }
}

TEST_CASE("dsl/ast: AST 落成 graph::Graph 且不丢信息") {
    const Result<AstGraph> ast = parse_flowchart(
        "flowchart LR\n A[读] -->|ok| B[写]\n B -.-> C[缓存]\n");
    REQUIRE(ast.has_value());

    const Result<graph::Graph> produced = to_graph(*ast);
    REQUIRE(produced.has_value());
    const graph::Graph& graph = *produced;
    REQUIRE(graph.nodes().size() == 3);
    REQUIRE(graph.edges().size() == 2);

    CHECK(graph.nodes()[0].label() == "读");
    CHECK(graph.nodes()[1].attributes().get("shape") == "box");
    CHECK(graph.edges()[0].label() == "ok");
    CHECK(graph.edges()[1].attributes().get("style") == "dashed");

    SUBCASE("指向不存在节点的 AST 被拒绝（手工构造也守得住）") {
        AstGraph broken;
        AstNode orphan;
        orphan.key = "A";
        orphan.label = "A";
        broken.nodes.push_back(orphan);
        AstEdge edge;
        edge.from = "A";
        edge.to = "缺失";
        broken.edges.push_back(edge);
        const Result<graph::Graph> rejected = to_graph(broken);
        REQUIRE_FALSE(rejected.has_value());
        CHECK(rejected.error() == ErrorCode::InvalidArgument);
    }
}

TEST_CASE("dsl/registry: function-local static + 快照语义") {
    DslRegistry& first = DslRegistry::instance();
    DslRegistry& second = DslRegistry::instance();
    CHECK(&first == &second);  // 同一定例（不是两份全局表）

    REQUIRE(register_builtin_dsls());
    CHECK(register_builtin_dsls());  // 幂等

    const DslDescriptor* mermaid = first.find("mermaid");
    REQUIRE(mermaid != nullptr);
    CHECK(mermaid->parse != nullptr);

    SUBCASE("按名解析") {
        const Result<AstGraph> ast = first.parse("flowchart", "flowchart TD\n A --> B");
        REQUIRE(ast.has_value());
        CHECK(ast->edges.size() == 1);
    }
    SUBCASE("未注册方言返回 Unsupported") {
        const Result<AstGraph> missing = first.parse("plantuml", "@startuml\n@enduml");
        REQUIRE_FALSE(missing.has_value());
        CHECK(missing.error() == ErrorCode::Unsupported);
    }
    SUBCASE("重名注册被拒绝（且大小写不敏感）") {
        const std::size_t before = first.size();
        DslDescriptor duplicate;
        duplicate.name = "MERMAID";
        duplicate.description = "重复";
        duplicate.directive = "flowchart";
        duplicate.parse = mermaid->parse;
        CHECK_FALSE(first.add(duplicate));
        CHECK(first.size() == before);
    }
    SUBCASE("空 parse 被拒绝") {
        DslDescriptor ghost;
        ghost.name = "ghost";
        ghost.description = "没有 parse 函数";
        CHECK_FALSE(first.add(ghost));
    }
    SUBCASE("快照是独立副本") {
        std::vector<DslDescriptor> snapshot = first.snapshot();
        REQUIRE(snapshot.size() == first.size());
        CHECK(snapshot[0].name == first.names()[0]);
    }
}

}  // namespace

// 夹具即生成物：sample_mermaid.txt 由 tools/gen_dsl_samples.py 产出，
// 这里断言"它恰好覆盖了受支持子集且能原样解析"——解析器支持面缩水时本用例会红。
#ifdef TERMCHART_FIXTURES_DIR
TEST_CASE("dsl/fixtures: sample_mermaid.txt 可解析且覆盖受支持子集") {
    std::ifstream input(std::filesystem::path(TERMCHART_FIXTURES_DIR) / "sample_mermaid.txt",
                        std::ios::binary);
    REQUIRE(input.good());
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string source = buffer.str();

    CHECK(detect_diagram(source) == MermaidDiagram::Flowchart);
    const Result<AstGraph> result = parse_mermaid(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;

    CHECK(ast.directive == "graph");
    CHECK(ast.direction == "LR");
    CHECK(ast.nodes.size() >= 9);
    CHECK(ast.edges.size() >= 9);

    REQUIRE(ast.find_node("C") != nullptr);
    CHECK(ast.find_node("C")->shape == "stadium");
    CHECK(ast.find_node("I")->shape == "hexagon");
    CHECK(ast.find_node("J")->shape == "asymmetric");
    CHECK(ast.find_node("H")->shape == "circle");

    const AstNode* retry = ast.find_node("D");
    REQUIRE(retry != nullptr);
    REQUIRE(retry->attributes.size() == 1);
    CHECK(retry->attributes[0].value == "重试阶段");

    // 被忽略的指令不应留下任何节点/边痕迹。
    CHECK(ast.find_node("classDef") == nullptr);
    CHECK(ast.find_node("click") == nullptr);

    const Result<graph::Graph> produced = to_graph(ast);
    REQUIRE(produced.has_value());
    CHECK(produced->nodes().size() == ast.nodes.size());
    CHECK(produced->edges().size() == ast.edges.size());
}
#endif
