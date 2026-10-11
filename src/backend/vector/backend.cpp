// backend/vector/backend.cpp —— 矢量子接口实现
#include "termchart/backend/vector/backend.hpp"

namespace termchart::backend::vector {

VectorBackend::~VectorBackend() = default;

std::string render_vector(VectorBackend& backend, const scene::Scene& scene) {
    backend.begin();
    backend.render(scene);
    backend.end();
    return backend.str();
}

}  // namespace termchart::backend::vector
