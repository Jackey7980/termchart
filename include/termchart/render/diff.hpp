// diff.hpp —— 差分重绘
//
// 纯函数层：输入两个 CellBuffer，输出"变化的连续段"。
// 合并规则：同一行内相邻/重叠的变化段合并成一段（多写一个字节的跳转序列
// 比多写十个字节的样式重置便宜得多）。
#ifndef TERMCHART_RENDER_DIFF_HPP
#define TERMCHART_RENDER_DIFF_HPP

#include <cstdint>
#include <vector>

#include "termchart/core/buffer.hpp"
#include "termchart/export.hpp"

namespace termchart::render {

struct DiffRegion {
    std::uint32_t row = 0;
    std::uint32_t begin = 0;   // 含
    std::uint32_t end = 0;     // 不含

    constexpr bool operator==(const DiffRegion&) const noexcept = default;
    [[nodiscard]] constexpr bool empty() const noexcept { return end <= begin; }
};

// 尺寸不同视为"全变"：返回 true（调用方应走全量重绘）。
TERMCHART_API bool identical(const CellBuffer& previous, const CellBuffer& current) noexcept;

// 逐行比较。size 不同返回空列表（调用方必须先处理尺寸变化）。
// merge_gap <= 0 表示"不合并"，> 0 表示相隔不超过该距离的段也合并。
TERMCHART_API std::vector<DiffRegion> diff(const CellBuffer& previous, const CellBuffer& current,
                                           std::uint32_t merge_gap = 4) noexcept;

}  // namespace termchart::render

#endif  // TERMCHART_RENDER_DIFF_HPP
