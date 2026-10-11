// backend/structured/html_backend.hpp —— Document → HTML
#ifndef TERMCHART_BACKEND_STRUCTURED_HTML_BACKEND_HPP
#define TERMCHART_BACKEND_STRUCTURED_HTML_BACKEND_HPP

#include "termchart/backend/structured/backend.hpp"

namespace termchart::backend::structured {

class HtmlBackend final : public StructuredBackend {
public:
    void render(const document::Document& doc) override;
};

}  // namespace termchart::backend::structured

#endif  // TERMCHART_BACKEND_STRUCTURED_HTML_BACKEND_HPP
