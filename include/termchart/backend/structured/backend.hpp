// backend/structured/backend.hpp —— 结构化物种子接口（Document → bytes）
//
// 结构化导出把 Document（语义块）翻成 HTML/Markdown 等"带结构"的文本格式。
// 输入是 Document 而非 chart：backend > chart 是禁止边，这就是它成立的原因。
#ifndef TERMCHART_BACKEND_STRUCTURED_BACKEND_HPP
#define TERMCHART_BACKEND_STRUCTURED_BACKEND_HPP

#include <string>
#include <string_view>

#include "termchart/backend/backend.hpp"
#include "termchart/document/document.hpp"
#include "termchart/export.hpp"

namespace termchart::backend::structured {

struct StructuredOptions {
    std::string title;
    std::string css_class = "termchart";
};

class StructuredBackend : public Backend {
public:
    ~StructuredBackend() override;

    void set_options(const StructuredOptions& options) noexcept { opts_ = options; }
    [[nodiscard]] const StructuredOptions& options() const noexcept { return opts_; }

    // 结构化物种的输入恒为 Document。
    virtual void render(const document::Document& doc) = 0;

    void begin() override { out_.clear(); }
    void end() override {}
    [[nodiscard]] std::string_view bytes() const override { return out_; }
    [[nodiscard]] const std::string& str() const noexcept { return out_; }

protected:
    StructuredOptions opts_;
    std::string out_;
};

[[nodiscard]] TERMCHART_API std::string render_structured(StructuredBackend& backend,
                                                          const document::Document& doc);

}  // namespace termchart::backend::structured

#endif  // TERMCHART_BACKEND_STRUCTURED_BACKEND_HPP
