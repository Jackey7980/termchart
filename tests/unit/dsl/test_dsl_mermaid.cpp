// test_dsl_mermaid.cpp —— dsl 模块：lexer / flowchart 解析 / AST→Graph / 注册表
//
// 关注的不是"能不能解析一句话"，而是**契约**：
//   * 非法输入不抛异常、不死循环、错误定位准确；
//   * 支持范围之外的东西要么报错、要么明确忽略，绝不静默错解；
//   * 注册表遵循 function-local static + 快照语义（与另四个注册表同构）。
#include <doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/dsl_registry.hpp"
#include "termchart/dsl/lexer.hpp"
#include "termchart/dsl/mermaid.hpp"
#include "termchart/dsl/mermaid_class.hpp"
#include "termchart/dsl/mermaid_flow.hpp"
#include "termchart/dsl/mermaid_sequence.hpp"
#include "termchart/dsl/mermaid_state.hpp"
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

    SUBCASE("序列图不被塞进图模型：Unsupported + 指向 parse_sequence") {
        ParseDiagnostic diagnostic;
        const Result<AstGraph> result = parse_mermaid("sequenceDiagram\n A->>B", {}, &diagnostic);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::Unsupported);
        CHECK(diagnostic.message.find("parse_sequence") != std::string::npos);
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

    // 枚举三件套（全仓注册表统一口径）：内置方言登记后必然非空。
    CHECK_FALSE(first.empty());
    CHECK(first.size() == first.names().size());
    // names() 按值返回——必须先落到局部再取迭代器，拿两个临时 vector 的
    // 迭代器互相比是 UB（Windows 上蒙对、macOS 上翻车过）。
    const std::vector<std::string_view> dsl_names = first.names();
    CHECK(std::find(dsl_names.begin(), dsl_names.end(), "mermaid") != dsl_names.end());

    const DslDescriptor* mermaid = first.find("mermaid");
    REQUIRE(mermaid != nullptr);
    CHECK(mermaid->parse != nullptr);

    SUBCASE("按名解析") {
        const Result<AstGraph> ast = first.parse("flowchart", "flowchart TD\n A --> B");
        REQUIRE(ast.has_value());
        CHECK(ast->edges.size() == 1);
    }
    SUBCASE("未注册方言返回 Unsupported") {
        // `yaml` 不在规格的方言清单里，是"永远不该被注册"的样本；
        // 已登记的方言（flowchart/class/state/dot/plantuml/d2）若在这里断言失败，
        // 说明注册面变了——那应该改的是期望值，而不是注册表。
        const Result<AstGraph> missing = first.parse("yaml", "x -> y");
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

// ── stateDiagram ──────────────────────────────────────────────────────────
TEST_CASE("dsl/state: 迁移、[*] 起止与组合态归属") {
    const std::string source = R"(stateDiagram-v2
        direction LR
        [*] --> 空闲
        空闲 --> 处理中: 收到任务
        处理中 --> 空闲: 完成
        处理中 --> [*]
        state 处理中 {
            校验 --> 提交
            提交 --> 校验
        }
        note right of 空闲 : 启动即进入
    )";
    const Result<AstGraph> result = parse_state_diagram(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;
    CHECK(ast.directive == "stateDiagram-v2");
    CHECK(ast.direction == "LR");

    // 伪状态 [*] 不建节点，语义落到真实状态上。
    CHECK(ast.find_node("[*]") == nullptr);
    const AstNode* idle = ast.find_node("空闲");
    REQUIRE(idle != nullptr);
    bool initial = false;
    bool final = false;
    bool note = false;
    for (const AstAttribute& attribute : idle->attributes) {
        initial = initial || attribute.key == "initial";
        note = note || attribute.key == "note";
    }
    const AstNode* busy = ast.find_node("处理中");
    REQUIRE(busy != nullptr);
    for (const AstAttribute& attribute : busy->attributes) {
        final = final || attribute.key == "final";
    }
    CHECK(initial);
    CHECK(final);
    CHECK(note);

    // 组合态：本身标记 composite，内部状态记 parent。
    bool composite = false;
    for (const AstAttribute& attribute : busy->attributes) {
        composite = composite || attribute.key == "composite";
    }
    CHECK(composite);
    const AstNode* inner = ast.find_node("校验");
    REQUIRE(inner != nullptr);
    REQUIRE_FALSE(inner->attributes.empty());
    CHECK(inner->attributes[0].value == "处理中");

    SUBCASE("缺头部报 ParseError") {
        ParseDiagnostic diagnostic;
        const Result<AstGraph> bad = parse_state_diagram("空闲 --> 处理中", {}, &diagnostic);
        REQUIRE_FALSE(bad.has_value());
        CHECK(bad.error() == ErrorCode::ParseError);
        CHECK(diagnostic.line == 1);
    }
    SUBCASE("组合态缺少 end 也要报错") {
        const Result<AstGraph> bad = parse_state_diagram("stateDiagram-v2\n state A {\n B\n");
        REQUIRE_FALSE(bad.has_value());
    }
}

// ── classDiagram ──────────────────────────────────────────────────────────
TEST_CASE("dsl/class: 关系方向语义（继承谁不能反）") {
    const std::string source = R"(classDiagram
        class Animal {
            +String name
            +eat()
        }
        Animal <|-- Dog
        Dog --|> Runnable
        Animal *-- Leg
        Animal o-- Tail
        Dog ..> Collar : uses
        Dog : +bark()
        class Runnable["可运行"]
    )";
    const Result<AstGraph> result = parse_class_diagram(source);
    REQUIRE(result.has_value());
    const AstGraph& ast = *result;

    const auto relation_of = [&ast](std::string_view from, std::string_view to) {
        for (const AstEdge& edge : ast.edges) {
            if (edge.from == from && edge.to == to) {
                for (const AstAttribute& attribute : edge.attributes) {
                    if (attribute.key == "relation") {
                        return attribute.value;
                    }
                }
            }
        }
        return std::string("missing");
    };

    // `A <|-- B`：B 继承 A → 边从 B 指向 A。
    CHECK(relation_of("Dog", "Animal") == "inheritance");
    CHECK(relation_of("Animal", "Dog") == "missing");
    // `A --|> B`：A 继承 B。
    CHECK(relation_of("Dog", "Runnable") == "inheritance");
    CHECK(relation_of("Animal", "Leg") == "composition");
    CHECK(relation_of("Animal", "Tail") == "aggregation");
    CHECK(relation_of("Dog", "Collar") == "dependency");

    const AstNode* animal = ast.find_node("Animal");
    REQUIRE(animal != nullptr);
    std::size_t members = 0;
    for (const AstAttribute& attribute : animal->attributes) {
        if (attribute.key == "member") {
            ++members;
        }
    }
    CHECK(members == 2);  // 类体两行

    const AstNode* dog = ast.find_node("Dog");
    REQUIRE(dog != nullptr);
    bool has_bark = false;
    for (const AstAttribute& attribute : dog->attributes) {
        has_bark = has_bark || attribute.value == "+bark()";
    }
    CHECK(has_bark);
    CHECK(ast.find_node("Runnable")->label == "可运行");
}

TEST_CASE("dsl/class: 多重度与构造型") {
    const std::string source = R"(classDiagram
        class Order {
            +int id
        }
        Order "1" --> "*" Item : contains
    )";
    const Result<AstGraph> result = parse_class_diagram(source);
    REQUIRE(result.has_value());
    REQUIRE(result->edges.size() == 1);
    const AstEdge& edge = result->edges[0];
    CHECK(edge.label == "contains");
    bool from_multi = false;
    bool to_multi = false;
    for (const AstAttribute& attribute : edge.attributes) {
        from_multi = from_multi || (attribute.key == "from_multiplicity" && attribute.value == "1");
        to_multi = to_multi || (attribute.key == "to_multiplicity" && attribute.value == "*");
    }
    CHECK(from_multi);
    CHECK(to_multi);

    SUBCASE("方向反转时多重度跟着端点换") {
        const Result<AstGraph> reversed =
            parse_class_diagram("classDiagram\n A \"1\" <|-- \"*\" B\n");
        REQUIRE(reversed.has_value());
        REQUIRE(reversed->edges.size() == 1);
        const AstEdge& edge2 = reversed->edges[0];
        CHECK(edge2.from == "B");
        CHECK(edge2.to == "A");
        for (const AstAttribute& attribute : edge2.attributes) {
            if (attribute.key == "from_multiplicity") {
                CHECK(attribute.value == "*");  // 挂在 B 上
            }
            if (attribute.key == "to_multiplicity") {
                CHECK(attribute.value == "1");  // 挂在 A 上
            }
        }
    }
}

// ── sequenceDiagram（独立类型，不进图模型） ───────────────────────────────
TEST_CASE("dsl/sequence: 参与者、消息种类与激活") {
    const std::string source = R"(sequenceDiagram
        participant A as 客户端
        actor B
        A->>B: 请求
        B-->>A: 响应
        B-)A: 通知
        A-xB: 丢弃
        activate B
        deactivate B
        Note left of A: 发起方
        Note over A,B: 交互区间
        loop 重试
            A->>B: 再来一次
        end
        autonumber
    )";
    const Result<AstSequence> result = parse_sequence(source);
    REQUIRE(result.has_value());
    const AstSequence& sequence = *result;
    CHECK(sequence.directive == "sequenceDiagram");
    REQUIRE(sequence.participants.size() == 2);
    CHECK(sequence.participants[0].label == "客户端");
    CHECK(sequence.participants[1].kind == "actor");

    REQUIRE(sequence.messages.size() == 5);
    CHECK(sequence.messages[0].kind == MessageKind::Async);
    CHECK(sequence.messages[1].kind == MessageKind::AsyncDotted);
    CHECK(sequence.messages[2].kind == MessageKind::Open);
    CHECK(sequence.messages[3].kind == MessageKind::Cross);
    CHECK(sequence.messages[0].text == "请求");
    CHECK(sequence.messages[0].order == 0);
    CHECK(sequence.messages[4].order == 4);

    REQUIRE(sequence.notes.size() == 2);
    CHECK(sequence.notes[0].placement == SequenceNote::Placement::Left);
    CHECK(sequence.notes[0].participants[0] == "A");
    CHECK(sequence.notes[1].placement == SequenceNote::Placement::Over);
    CHECK(sequence.notes[1].participants.size() == 2);

    SUBCASE("块语句与 autonumber 只登记不报错") {
        bool saw_block = false;
        bool saw_autonumber = false;
        for (const AstAttribute& attribute : sequence.attributes) {
            saw_block = saw_block || attribute.key == "ignored_block";
            saw_autonumber = saw_autonumber || attribute.key == "autonumber";
        }
        CHECK(saw_block);
        CHECK(saw_autonumber);
    }
    SUBCASE("激活简写") {
        const Result<AstSequence> shorthand = parse_sequence(
            "sequenceDiagram\n A->>+B: 开\n B-->>-A: 关\n");
        REQUIRE(shorthand.has_value());
        REQUIRE(shorthand->messages.size() == 2);
        CHECK(shorthand->messages[0].activate_target);
        CHECK(shorthand->messages[1].deactivate_source);
    }
    SUBCASE("缺接收者报 ParseError") {
        const Result<AstSequence> bad = parse_sequence("sequenceDiagram\n A->>");
        REQUIRE_FALSE(bad.has_value());
        CHECK(bad.error() == ErrorCode::ParseError);
    }
    SUBCASE("消息种类有稳定名字（渲染器与文档共用）") {
        CHECK(std::string(message_kind_name(MessageKind::AsyncDotted)) == "async-dotted");
    }
}

TEST_CASE("dsl/mermaid: class/state 走 parse_mermaid，sequence 明确指向专用入口") {
    const Result<AstGraph> class_ast = parse_mermaid("classDiagram\n A <|-- B\n");
    REQUIRE(class_ast.has_value());
    CHECK(class_ast->edges.size() == 1);

    const Result<AstGraph> state_ast = parse_mermaid("stateDiagram-v2\n [*] --> A\n");
    REQUIRE(state_ast.has_value());
    CHECK(state_ast->find_node("A") != nullptr);

    ParseDiagnostic diagnostic;
    const Result<AstGraph> sequence_ast =
        parse_mermaid("sequenceDiagram\n A->>B: hi\n", {}, &diagnostic);
    REQUIRE_FALSE(sequence_ast.has_value());
    CHECK(sequence_ast.error() == ErrorCode::Unsupported);
    CHECK(diagnostic.message.find("parse_sequence") != std::string::npos);
}

TEST_CASE("dsl/registry: class/state 已登记，sequence 刻意不登记") {
    DslRegistry& registry = DslRegistry::instance();
    (void)register_builtin_dsls();
    CHECK(registry.find("class") != nullptr);
    CHECK(registry.find("state") != nullptr);
    CHECK(registry.find("sequence") == nullptr);  // 产物不是 AstGraph，契约不兼容
    CHECK(registry.parse("class", "classDiagram\n A <|-- B").has_value());
    CHECK(registry.parse("state", "stateDiagram-v2\n [*] --> A").has_value());
}
