// backend/snapshot/backend.hpp —— 快照物种子接口（CellBuffer → bytes）
//
// 快照导出是一次性的：无 diff、无 prev 状态、可重入。所有具体后端（ansi/text/
// html_pre/json）都只是把同一个 CellBuffer 用不同字节流表达。
#ifndef TERMCHART_BACKEND_SNAPSHOT_BACKEND_HPP
#define TERMCHART_BACKEND_SNAPSHOT_BACKEND_HPP

#include <string>
#include <string_view>

#include "termchart/backend/backend.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/color_depth.hpp"
#include "termchart/export.hpp"

namespace termchart::backend::snapshot {

struct SnapshotOptions {
    ColorDepth depth = ColorDepth::TrueColor;  // ansi 后端用；text/json 忽略
    bool trim_trailing_blank = true;           // 行尾"空白且无样式"裁掉
};

class SnapshotBackend : public Backend {
public:
    ~SnapshotBackend() override;

    void set_options(const SnapshotOptions& options) noexcept { opts_ = options; }
    [[nodiscard]] const SnapshotOptions& options() const noexcept { return opts_; }

    // 快照物种的输入恒为 CellBuffer。
    virtual void render(const CellBuffer& buffer) = 0;

    void begin() override { out_.clear(); }
    void end() override {}
    [[nodiscard]] std::string_view bytes() const override { return out_; }
    [[nodiscard]] const std::string& str() const noexcept { return out_; }

protected:
    // 该行有效宽度：trim_trailing_blank 时裁掉行尾"空白且无样式"的格。
    [[nodiscard]] std::uint32_t row_width(const CellBuffer& buffer,
                                          std::uint32_t y) const noexcept;

    SnapshotOptions opts_;
    std::string out_;
};

// 便捷：begin + render + end，返回字节流拷贝。
[[nodiscard]] TERMCHART_API std::string render_snapshot(SnapshotBackend& backend,
                                                        const CellBuffer& buffer);

}  // namespace termchart::backend::snapshot

#endif  // TERMCHART_BACKEND_SNAPSHOT_BACKEND_HPP
