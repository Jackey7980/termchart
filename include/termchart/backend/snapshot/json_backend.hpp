// backend/snapshot/json_backend.hpp —— CellBuffer + 属性 → 结构化 JSON
//
// 逐格序列化：码点、UTF-8 字符、前/背景（含 kind/index/rgb）、属性位掩码。
// 供机器消费（比如把终端产物喂给前端做 diff 比对）。
#ifndef TERMCHART_BACKEND_SNAPSHOT_JSON_BACKEND_HPP
#define TERMCHART_BACKEND_SNAPSHOT_JSON_BACKEND_HPP

#include "termchart/backend/snapshot/backend.hpp"

namespace termchart::backend::snapshot {

class JsonBackend final : public SnapshotBackend {
public:
    void render(const CellBuffer& buffer) override;
};

}  // namespace termchart::backend::snapshot

#endif  // TERMCHART_BACKEND_SNAPSHOT_JSON_BACKEND_HPP
