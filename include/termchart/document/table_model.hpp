// table_model.hpp —— 表格的语义模型（行列字符串，非像素）
//
// backend/structured 负责把它排版成 HTML/Markdown；本层只表达"表头 + 若干行"。
// header-only 值类型。
#ifndef TERMCHART_DOCUMENT_TABLE_MODEL_HPP
#define TERMCHART_DOCUMENT_TABLE_MODEL_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace termchart::document {

class TableModel {
public:
    void set_header(std::vector<std::string> header) { header_ = std::move(header); }
    void add_row(std::vector<std::string> row) { rows_.push_back(std::move(row)); }

    [[nodiscard]] const std::vector<std::string>& header() const noexcept { return header_; }
    [[nodiscard]] const std::vector<std::vector<std::string>>& rows() const noexcept {
        return rows_;
    }
    [[nodiscard]] std::size_t row_count() const noexcept { return rows_.size(); }
    [[nodiscard]] std::size_t column_count() const noexcept {
        std::size_t n = header_.size();
        for (const auto& r : rows_) {
            if (r.size() > n) {
                n = r.size();
            }
        }
        return n;
    }
    [[nodiscard]] bool empty() const noexcept { return header_.empty() && rows_.empty(); }

private:
    std::vector<std::string> header_;
    std::vector<std::vector<std::string>> rows_;
};

}  // namespace termchart::document

#endif  // TERMCHART_DOCUMENT_TABLE_MODEL_HPP
