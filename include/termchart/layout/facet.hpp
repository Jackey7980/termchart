// facet.hpp —— 纯几何的矩形划分（facet 布局）
//
// 只做一件事：把一块矩形按 rows × cols 切成带间距的小矩形。**不建图、不画图**，
// 也不认识 chart —— 调用方拿到 Rect 列表后自己决定每格画什么（这正是分面图与
// 仪表盘拼版需要的原语，也让本文件可以被 GUI/布局层无条件复用）。
//
// 两个约定：
//   * 行优先：索引 row * cols + col，facet(0,0) 在左上角；
//   * 尺寸除不尽时，余数按"从左到右、从上到下"逐格 +1 分配，各格尺寸最多相差
//     一格；分配后宽度或高度不足 1 格的格子返回空矩形（w 或 h 为 0），
//     由调用方跳过 —— 不产生"看起来有其实画不下"的格子。
#ifndef TERMCHART_LAYOUT_FACET_HPP
#define TERMCHART_LAYOUT_FACET_HPP

#include <cstdint>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"

namespace termchart::layout {

struct FacetOptions {
    std::uint32_t rows = 1;
    std::uint32_t cols = 1;
    double gap_x = 1.0;  // 列间距（格）
    double gap_y = 1.0;  // 行间距（格）
};

// 全部格子（行优先）。rows 或 cols 为 0 时返回空列表。
[[nodiscard]] TERMCHART_API std::vector<Rect> split_rect(
    Rect area, const FacetOptions& options = {});

// 单个格子；越界返回空矩形。
[[nodiscard]] TERMCHART_API Rect facet_cell(Rect area, const FacetOptions& options,
                                            std::uint32_t row, std::uint32_t col);

}  // namespace termchart::layout

#endif  // TERMCHART_LAYOUT_FACET_HPP
