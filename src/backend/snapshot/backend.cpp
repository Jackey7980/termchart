// backend/snapshot/backend.cpp —— 快照子接口实现
#include "termchart/backend/snapshot/backend.hpp"

#include "termchart/core/cell.hpp"

namespace termchart::backend::snapshot {

SnapshotBackend::~SnapshotBackend() = default;

std::uint32_t SnapshotBackend::row_width(const CellBuffer& buffer,
                                         std::uint32_t y) const noexcept {
    std::uint32_t w = buffer.width();
    if (!opts_.trim_trailing_blank) {
        return w;
    }
    while (w > 0 && is_blank(buffer.at(w - 1, y))) {
        --w;
    }
    return w;
}

std::string render_snapshot(SnapshotBackend& backend, const CellBuffer& buffer) {
    backend.begin();
    backend.render(buffer);
    backend.end();
    return backend.str();
}

}  // namespace termchart::backend::snapshot
