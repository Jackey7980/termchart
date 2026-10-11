// backend/structured/markdown_backend.hpp —— Document → Markdown
#ifndef TERMCHART_BACKEND_STRUCTURED_MARKDOWN_BACKEND_HPP
#define TERMCHART_BACKEND_STRUCTURED_MARKDOWN_BACKEND_HPP

#include "termchart/backend/structured/backend.hpp"

namespace termchart::backend::structured {

class MarkdownBackend final : public StructuredBackend {
public:
    void render(const document::Document& doc) override;
};

}  // namespace termchart::backend::structured

#endif  // TERMCHART_BACKEND_STRUCTURED_MARKDOWN_BACKEND_HPP
