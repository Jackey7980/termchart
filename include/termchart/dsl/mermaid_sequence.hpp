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
//   `loop`/`alt`/`else`/`opt`/`par`/`critical`/`rect`/`break` 块语句：块标签、嵌套深度、
//   分支（alt 的 else / par 的 and）以及每条消息的归属都进 AST（见 SequenceBlock）
//   `%%` 注释
// ⛔ 认出但忽略：`autonumber`、`title`、`box`、`link`、`links`、`create`/`destroy`
//   参与者、`participant` 的样式参数
// ⛔ 不支持：递归激活栈的精确配对
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

// 块里的一个分支。alt 的每个 else、par 的每个 and 都是一个分支；主分支 keyword 为空。
struct SequenceBranch {
    std::string keyword;             // ""=主分支 / "else" / "and"
    std::string label;               // 分支条件原文（alt 的 "是成功"、loop 的 "Every 10ms"）
    std::size_t first_message = 0;   // 本分支第一条消息在 AstSequence::messages 的下标
    std::size_t message_count = 0;   // 本分支消息数
};

// 序列图里的块（loop / alt / opt / par / critical / rect / break）。
// 原先块语句被整行跳过——连 `loop Every 10ms` 的标签文本都丢了，画不出条件/循环框。
// 现在块的结构完整进 AST：标签、嵌套深度、分支，以及每条消息的归属。
struct SequenceBlock {
    std::string keyword;             // loop / alt / opt / par / critical / rect / break
    std::string label;               // 块标签（loop 的 "Every 10ms" 之类；alt/par 为空）
    std::size_t depth = 0;           // 嵌套深度（0 = 顶层）
    std::vector<SequenceBranch> branches;  // 至少一个：主分支在前
};

struct SequenceMessage {
    std::string from;
    std::string to;
    std::string text;
    MessageKind kind = MessageKind::Solid;
    std::size_t order = 0;       // 0-based 出现次序（渲染时的时间轴）
    bool activate_target = false;
    bool deactivate_source = false;
    std::string block;           // 归属块路径（"loop" / "alt" / "alt/else"）；空 = 不在块里
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
    std::vector<SequenceBlock> blocks;      // 按**开启顺序**排列；depth 给出嵌套层级
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
