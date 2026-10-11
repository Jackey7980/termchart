// backend/vector/svg_backend.hpp —— Scene → SVG
#ifndef TERMCHART_BACKEND_VECTOR_SVG_BACKEND_HPP
#define TERMCHART_BACKEND_VECTOR_SVG_BACKEND_HPP

#include "termchart/backend/vector/backend.hpp"

namespace termchart::backend::vector {

class SvgBackend final : public VectorBackend {
public:
    void render(const scene::Scene& scene) override;
};

}  // namespace termchart::backend::vector

#endif  // TERMCHART_BACKEND_VECTOR_SVG_BACKEND_HPP
