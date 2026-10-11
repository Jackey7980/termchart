#include "termchart/render/diff.hpp"

#include <utility>

#include "termchart/core/span_utils.hpp"

namespace termchart::render {

bool identical(const CellBuffer& previous, const CellBuffer& current) noexcept {
    if (previous.width() != current.width() || previous.height() != current.height()) {
        return false;
    }
    if (previous.side_entry_count() != current.side_entry_count()) {
        return false;
    }
    return span_equal(std::span<const Cell>(previous.cells()),
                      std::span<const Cell>(current.cells()));
}

std::vector<DiffRegion> diff(const CellBuffer& previous, const CellBuffer& current,
                             std::uint32_t merge_gap) noexcept {
    std::vector<DiffRegion> out;
    if (previous.width() != current.width() || previous.height() != current.height()) {
        return out;  // 尺寸变化由调用方走全量重绘
    }
    const std::uint32_t width = current.width();

    for (std::uint32_t row = 0; row < current.height(); ++row) {
        const std::span<const Cell> before = previous.row(row);
        const std::span<const Cell> after = current.row(row);

        // 1) 找出所有"连续变化段"。
        std::vector<std::pair<std::uint32_t, std::uint32_t>> raw;
        std::uint32_t start = 0;
        bool in_run = false;
        for (std::uint32_t col = 0; col < width; ++col) {
            const bool changed = !(before[col] == after[col]);
            if (changed && !in_run) {
                start = col;
                in_run = true;
            } else if (!changed && in_run) {
                raw.emplace_back(start, col);
                in_run = false;
            }
        }
        if (in_run) {
            raw.emplace_back(start, width);
        }

        // 2) 合并相隔不超过 merge_gap 的段。
        for (const auto& [begin, end] : raw) {
            if (!out.empty() && out.back().row == row && begin <= out.back().end + merge_gap) {
                out.back().end = end;
            } else {
                out.push_back(DiffRegion{row, begin, end});
            }
        }
    }
    return out;
}

}  // namespace termchart::render
