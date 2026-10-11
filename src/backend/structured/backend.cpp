// backend/structured/backend.cpp —— 结构化物种子接口实现
#include "termchart/backend/structured/backend.hpp"

namespace termchart::backend::structured {

StructuredBackend::~StructuredBackend() = default;

std::string render_structured(StructuredBackend& backend, const document::Document& doc) {
    backend.begin();
    backend.render(doc);
    backend.end();
    return backend.str();
}

}  // namespace termchart::backend::structured
