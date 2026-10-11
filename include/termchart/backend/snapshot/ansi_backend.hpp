// backend/snapshot/ansi_backend.hpp —— CellBuffer → 完整 ANSI 字节流
//
// **唯一**允许 include render/sgr 的 backend 文件（收窄边，见 TermchartModules.cmake）。
// 一次性、无 diff、无 prev 状态、可重入；不自建 SGR——序列化规则改动只在 sgr.hpp。
#ifndef TERMCHART_BACKEND_SNAPSHOT_ANSI_BACKEND_HPP
#define TERMCHART_BACKEND_SNAPSHOT_ANSI_BACKEND_HPP

#include "termchart/backend/snapshot/backend.hpp"

namespace termchart::backend::snapshot {

class AnsiBackend final : public SnapshotBackend {
public:
    void render(const CellBuffer& buffer) override;
};

}  // namespace termchart::backend::snapshot

#endif  // TERMCHART_BACKEND_SNAPSHOT_ANSI_BACKEND_HPP
