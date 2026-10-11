// document.hpp —— Document：内容块容器（structured 物种的输入）
//
// Document 是 backend/structured 的输入：它只表达语义（段落/标题/表格/图表），
// 不含任何渲染或像素决策。`to_plain_text()` 是给测试与 text 导出的最小序列化，
// 真正的 HTML/Markdown 由 backend/structured/* 负责。
#ifndef TERMCHART_DOCUMENT_DOCUMENT_HPP
#define TERMCHART_DOCUMENT_DOCUMENT_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "termchart/document/block.hpp"
#include "termchart/export.hpp"

namespace termchart::document {

class Document {
public:
    void add_block(Block block) { blocks_.push_back(std::move(block)); }

    [[nodiscard]] const std::vector<Block>& blocks() const noexcept { return blocks_; }
    [[nodiscard]] std::size_t size() const noexcept { return blocks_.size(); }
    [[nodiscard]] bool empty() const noexcept { return blocks_.empty(); }
    [[nodiscard]] BlockKind kind_of(std::size_t index) const;

    // 最小纯文本序列化：标题/段落/代码/表格/图表占位，便于快照与测试。
    [[nodiscard]] TERMCHART_API std::string to_plain_text() const;

private:
    std::vector<Block> blocks_;
};

}  // namespace termchart::document

#endif  // TERMCHART_DOCUMENT_DOCUMENT_HPP
