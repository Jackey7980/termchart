// ast.hpp —— DSL 解析结果的中立模型（L17）
//
// 设计要点：AST **不依赖任何绘图类型**，但字段与 `graph::Graph` 一一对应，
// 于是"方言 → AST → Graph"是一条没有信息损失的单向路（见 to_graph）。
//
// 为什么不让方言直接吐 Graph：
//   1. 同一张 AST 可以被吐成图、表格、树三种产物（table_markdown / csv_sniff 也要用）；
//   2. 解析期的诊断需要带原文位置，而 Graph 里没有位置概念；
//   3. 后续 DSL（plantuml / d2）的差异可以只在 AST 上体现，下游零改动。
#ifndef TERMCHART_DSL_AST_HPP
#define TERMCHART_DSL_AST_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/core/error.hpp"
#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"
#include "termchart/graph/graph.hpp"

// 注意：这里 include graph/graph.hpp 而不是前向声明——`to_graph()` 的返回类型是
// `Result<graph::Graph>`，而 `Expected<T, E>` 内部持有 `std::variant<T, E>`，
// T 不完整就无法实例化（任何调用 to_graph 的 TU 都会撞上）。层号上 dsl(17) ≥
// graph(13)，这个方向本来就是允许的。

namespace termchart::dsl {

// 箭头语义。数值与 graph::ArrowStyle 对齐由 to_graph 负责，AST 自己不 include graph。
enum class ArrowKind : std::uint8_t {
    None = 0,      // ---
    AtTarget = 1,  // -->
    Both = 2,      // <-->
};

// 开放键值袋：方言里"不认识但已解析"的属性都落在这里，绝不静默丢弃。
struct AstAttribute {
    std::string key;
    std::string value;
};

struct AstNode {
    std::string key;      // 方言内的标识符（mermaid 的 A、dot 的 node_1）
    std::string label;    // 显示文本；方言没写标签时等于 key
    std::string shape;    // "box"/"round"/"stadium"/"diamond"/"hexagon"/"circle"/""（空 = 方言默认）
    std::vector<AstAttribute> attributes;
};

struct AstEdge {
    std::string from;                          // 引用 AstNode::key
    std::string to;
    std::string label;
    ArrowKind arrow = ArrowKind::AtTarget;
    std::string line_style;                    // ""(实线) / "dashed" / "thick"
    std::vector<AstAttribute> attributes;
};

struct AstGraph {
    std::string name;        // 图名（digraph G / flowchart 无名时为空）
    std::string directive;   // 原文指令，例如 "flowchart TD"、"digraph"
    std::string direction;   // "TD"/"LR"/... 或空；由渲染侧决定用哪个布局
    // 图级元信息：方言里"不属于任何节点"的属性（D2 的顶层 `label:`、rankdir 原文等）。
    // 放在图上而不是塞进第一个节点，避免凭空造出幽灵节点。
    std::vector<AstAttribute> attributes;
    std::vector<AstNode> nodes;
    std::vector<AstEdge> edges;

    // 两份重载（const / 非 const）：解析器要在找到的节点上继续填字段，
    // 历史上只能 `const_cast<AstNode*>(find_node(...))`，散在 8 处方言文件里。
    // 现在非 const 版本是第一等公民，const_cast 只保留在实现内部那一处。
    [[nodiscard]] TERMCHART_API const AstNode* find_node(std::string_view key) const noexcept;
    [[nodiscard]] TERMCHART_API AstNode* find_node(std::string_view key) noexcept;
    [[nodiscard]] TERMCHART_API bool has_node(std::string_view key) const noexcept;
    // 找不到就创建（解析期的常见动作：边端点可能先于声明出现）。
    TERMCHART_API AstNode& ensure_node(std::string_view key);
};

// AST → Graph 的**唯一**适配点。alias 与 style 类属性直接写进 Attributes。
// 失败只可能来自"边引用了不存在的节点"（解析器已保证不会发生，因此这是内部不变量）。
[[nodiscard]] TERMCHART_API Result<graph::Graph> to_graph(const AstGraph& ast);

}  // namespace termchart::dsl

#endif  // TERMCHART_DSL_AST_HPP
