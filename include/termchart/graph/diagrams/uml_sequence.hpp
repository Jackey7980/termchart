// uml_sequence.hpp —— UML 时序图（参与者头框 + 生命线 + 消息箭头 + 便签 + 激活条）
//
// 字符化策略：参与者画成顶部一排**头框**（矩形 + 居中名），框中心列向下引
// `│` 生命线；消息画在两条生命线之间的水平线上——实线 `─` / 虚线 `┄`，箭头按
// 语义选形：实心 ▶/◀、开口 ▷/◁、叉 ×（丢失消息）；Note 按 left/right/over
// 落在生命线旁或跨在两条生命线之间，整段用 [方括号] 包住与消息文本区分；
// 激活条画成生命线上的 `█` 段。
//
// 本层自持模型（graph(13) 不得 include dsl(17)，故不复用 AstSequence；
// dsl/mermaid_sequence 的 AstSequence 可机械映射到本模型，由上层适配：
//   SequenceParticipant/SequenceMessage/SequenceNote 的字段一一对应）。
// 参与者按 key（字符串）引用；渲染时未知 key 的消息跳过、便签退化为 over 单人。
#ifndef TERMCHART_GRAPH_DIAGRAMS_UML_SEQUENCE_HPP
#define TERMCHART_GRAPH_DIAGRAMS_UML_SEQUENCE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

enum class SeqArrowKind : std::uint8_t {
    Solid = 0,     //  实线 + 实心箭头（同步调用）
    Dotted,        //  虚线 + 实心箭头（同步返回）
    Open,          //  实线 + 开口箭头（异步）
    OpenDotted,    //  虚线 + 开口箭头
    Cross,         //  实线 + 叉箭头（丢失消息）
    CrossDotted,   //  虚线 + 叉箭头
};

struct SeqParticipant {
    std::string key;     // 引用名（消息/便签用它指认参与者）
    std::string label;   // 显示名；空则回退 key
    bool actor = false;  // true：小人人设（保留字段；本渲染器统一画头框）
};

struct SeqMessage {
    std::string from;
    std::string to;
    std::string text;
    SeqArrowKind kind = SeqArrowKind::Solid;
    bool activate_target = false;    // 到达后激活 to（Mermaid `+` 简写）
    bool deactivate_source = false;  // 发出前停用 from（Mermaid `-` 简写）
};

struct SeqNote {
    enum class Placement : std::uint8_t { Left, Right, Over };
    Placement placement = Placement::Over;
    std::string anchor;    // Left/Right：单参与者；Over 单人时也用它
    std::string anchor_b;  // Over 跨两人时的第二人
    std::string text;
    std::size_t after = static_cast<std::size_t>(-1);  // 出现在第 after 条消息之后
};

struct SeqModel {
    std::vector<SeqParticipant> participants;
    std::vector<SeqMessage> messages;
    std::vector<SeqNote> notes;

    // key → 参与者下标；未找到返回 size_t(-1)。
    [[nodiscard]] TERMCHART_API std::size_t index_of(std::string_view key) const noexcept;
};

struct UmlSequenceOptions {
    double h_gap = 8.0;    // 头框之间的间隔（给消息与文本留位）
    double margin = 1.0;   // 左边距
    Style header_style{};    // 头框
    Style lifeline_style{};  // 生命线与激活条
    Style message_style{};   // 消息线、箭头与文本
    Style note_style{};      // 便签
};

// 就地绘制到画布（超出画布的部分由 canvas 裁剪）。返回内容包围盒；空模型返回零矩形。
[[nodiscard]] TERMCHART_API Rect render_uml_sequence(const SeqModel& model,
                                                     canvas::Canvas& canvas,
                                                     const UmlSequenceOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_UML_SEQUENCE_HPP
