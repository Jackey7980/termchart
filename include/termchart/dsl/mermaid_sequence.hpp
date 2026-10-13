// mermaid_sequence.hpp —— Mermaid sequenceDiagram（L17）
//
// **为什么这里不是 AstGraph**：序列图的时间轴语义（消息顺序、激活、生命线）
// 无法用"节点 + 边"无损表达——硬塞进去会得到一个既不是图、也不能画序列图的
// 半成品。这与本库对"三物种后端不做跨渲染模型转换"的态度一致：
// 模型不匹配就承认，不硬套。
//
// 因此本方言导出**自己的类型** `AstSequence`，并且**不登记进 DslRegistry**
// （注册表的契约是 `Result<AstGraph>`，见 dsl_registry.hpp）。序列图渲染器落地时
// 直接消费 `AstSequence` 即可，不需要改动本层。
//
// ✅ 支持
//   `sequenceDiagram` 头部
//   `participant A as 别名` / `actor A` / `database A`
//   `A->>B: 文本`、`A-->>B: 文本`（虚线）、`A-)B`（开口）、`A-xB`（叉）、
//   `A->B` / `A-->B`（无箭头），后接 `+` / `-` 的激活简写
//   `activate A` / `deactivate A`（记到**上一条消息**的 `activate`/`deactivate`）
//   `Note left of A: 文本` / `Note right of A: …` / `Note over A,B: …`
//   `%%` 注释
// ⛔ 认出但忽略：`loop`/`alt`/`else`/`opt`/`par`/`end` 等块语句（不展开嵌套），
//   `autonumber`、`title`、`box`、`link`、`links`、`participant` 的样式参数
// ⛔ 不支持：`create`/`destroy` 参与者、`break`、递归激活栈的精确配对
#ifndef TERMCHART_DSL_MERMAID_SEQUENCE_HPP
#define TERMCHART_DSL_MERMAID_SEQUENCE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/core/expected.hpp"
#include "termchart/dsl/ast.hpp"
#include "termchart/dsl/parser_base.hpp"
#include "termchart/export.hpp"

namespace termchart::dsl {

enum class MessageKind : std::uint8_t {
    Solid = 0,     // ->
    Dotted = 1,    // -->
    Async = 2,     // ->>
    AsyncDotted = 3,  // -->>
    Open = 4,      // -)
    OpenDotted = 5,   // --)
    Cross = 6,     // -x
    CrossDotted = 7,  // --x
};

struct SequenceParticipant {
    std::string key;      // 原文标识符
    std::string label;    // 显示名（`as` 后，缺省等于 key）
    std::string kind;     // "participant" / "actor" / "database" / ...
    std::vector<AstAttribute> attributes;
};

struct SequenceMessage {
    std::string from;
    std::string to;
    std::string text;
    MessageKind kind = MessageKind::Solid;
    std::size_t order = 0;       // 0-based 出现次序（渲染时的时间轴）
    bool activate_target = false;
    bool deactivate_source = false;
};

struct SequenceNote {
    enum class Placement : std::uint8_t { Left, Right, Over };
    Placement placement = Placement::Over;
    std::vector<std::string> participants;   // 1~2 个；over 可跨两人
    std::string text;
};

struct AstSequence {
    std::string directive;      // `sequenceDiagram`
    std::string title;          // `title …`（若有）
    std::vector<SequenceParticipant> participants;
    std::vector<SequenceMessage> messages;
    std::vector<SequenceNote> notes;
    std::vector<AstAttribute> attributes;   // 未识别但已登记的元信息

    // 两份重载（const / 非 const）：理由同 AstGraph::find_node——非 const 版本是
    // 第一等公民，解析器要改的时候不必在各处 const_cast。
    [[nodiscard]] TERMCHART_API const SequenceParticipant* find_participant(
        std::string_view key) const noexcept;
    [[nodiscard]] TERMCHART_API SequenceParticipant* find_participant(
        std::string_view key) noexcept;
    [[nodiscard]] TERMCHART_API bool has_participant(std::string_view key) const noexcept;
    TERMCHART_API SequenceParticipant& ensure_participant(std::string_view key);
};

struct SequenceParseOptions {
    std::size_t max_participants = 1000;
    std::size_t max_messages = 100000;
};

[[nodiscard]] TERMCHART_API Result<AstSequence> parse_sequence(
    std::string_view source, const SequenceParseOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

[[nodiscard]] TERMCHART_API const char* message_kind_name(MessageKind kind) noexcept;

inline constexpr std::string_view kSequenceDirective = "sequenceDiagram";

}  // namespace termchart::dsl

#endif  // TERMCHART_DSL_MERMAID_SEQUENCE_HPP
