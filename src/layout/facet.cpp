// facet.cpp —— 矩形划分实现
#include "termchart/layout/facet.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace termchart::layout {
namespace {

// 把 total 按 count 份均分，余数逐份 +1；每份再扣除 gap 之外的空隙总量。
// 返回各份的长度（可能为负，调用方会截成空矩形）。
std::vector<double> distribute(double total, std::uint32_t count, double gap) {
    std::vector<double> sizes;
    if (count == 0) {
        return sizes;
    }
    const double gaps = gap * static_cast<double>(count - 1);
    const double usable = total - gaps;
    if (usable <= 0.0) {
        sizes.assign(count, 0.0);
        return sizes;
    }
    const double base = std::floor(usable / static_cast<double>(count));
    double remainder = usable - base * static_cast<double>(count);
    sizes.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        double size = base;
        if (remainder >= 1.0) {
            size += 1.0;  // 余数逐格 +1，保证各格最多相差一格
            remainder -= 1.0;
        }
        sizes.push_back(size);
    }
    return sizes;
}

}  // namespace

std::vector<Rect> split_rect(Rect area, const FacetOptions& options) {
    std::vector<Rect> cells;
    if (options.rows == 0 || options.cols == 0) {
        return cells;
    }
    const std::vector<double> widths = distribute(area.w, options.cols, options.gap_x);
    const std::vector<double> heights = distribute(area.h, options.rows, options.gap_y);

    cells.reserve(static_cast<std::size_t>(options.rows) * options.cols);
    double y = area.y;
    for (std::uint32_t row = 0; row < options.rows; ++row) {
        double x = area.x;
        for (std::uint32_t col = 0; col < options.cols; ++col) {
            const double w = widths[col];
            const double h = heights[row];
            if (w < 1.0 || h < 1.0) {
                cells.push_back(Rect{});  // 放不下：空矩形，调用方跳过
            } else {
                cells.push_back(Rect{x, y, w, h});
            }
            x += w + options.gap_x;
        }
        y += heights[row] + options.gap_y;
    }
    return cells;
}

Rect facet_cell(Rect area, const FacetOptions& options, std::uint32_t row,
                std::uint32_t col) {
    if (row >= options.rows || col >= options.cols) {
        return Rect{};
    }
    return split_rect(area, options)[static_cast<std::size_t>(row) * options.cols + col];
}

}  // namespace termchart::layout
