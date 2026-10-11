// block.hpp —— Document 的内容块（语义单元）
//
// 一个 Block 是 Paragraph / Heading / Code / Table / Chart 之一。Chart 块持有
// Scene（矢量 IR）——于是"图表"作为文档元素不需要 backend 认识 chart 类型，
// 这正是 backend > chart 禁止边成立的前提。
#ifndef TERMCHART_DOCUMENT_BLOCK_HPP
#define TERMCHART_DOCUMENT_BLOCK_HPP

#include <string>
#include <vector>

#include "termchart/document/span.hpp"
#include "termchart/document/table_model.hpp"
#include "termchart/export.hpp"
#include "termchart/scene/scene.hpp"

namespace termchart::document {

enum class BlockKind {
    Paragraph,
    Heading,
    Code,
    Table,
    Chart,
};

class Block {
public:
    Block() = default;

    static Block paragraph(std::vector<Span> spans);
    static Block heading(std::string text, int level = 1);
    static Block code(std::string text, std::string language = {});
    static Block table(TableModel model);
    static Block chart(scene::Scene scene);

    [[nodiscard]] BlockKind kind() const noexcept { return kind_; }
    [[nodiscard]] const std::vector<Span>& spans() const noexcept { return spans_; }
    [[nodiscard]] const std::string& text() const noexcept { return text_; }
    [[nodiscard]] const std::string& language() const noexcept { return language_; }
    [[nodiscard]] int level() const noexcept { return level_; }
    [[nodiscard]] const TableModel& table() const noexcept { return table_; }
    [[nodiscard]] const scene::Scene& scene() const noexcept { return scene_; }

private:
    BlockKind kind_ = BlockKind::Paragraph;
    std::vector<Span> spans_;
    std::string text_;
    std::string language_;
    int level_ = 0;
    TableModel table_;
    scene::Scene scene_;
};

}  // namespace termchart::document

#endif  // TERMCHART_DOCUMENT_BLOCK_HPP
