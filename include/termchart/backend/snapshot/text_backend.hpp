// backend/snapshot/text_backend.hpp —— CellBuffer → 纯文本（无 SGR）
#ifndef TERMCHART_BACKEND_SNAPSHOT_TEXT_BACKEND_HPP
#define TERMCHART_BACKEND_SNAPSHOT_TEXT_BACKEND_HPP

#include "termchart/backend/snapshot/backend.hpp"

namespace termchart::backend::snapshot {

class TextBackend final : public SnapshotBackend {
public:
    void render(const CellBuffer& buffer) override;
};

}  // namespace termchart::backend::snapshot

#endif  // TERMCHART_BACKEND_SNAPSHOT_TEXT_BACKEND_HPP
