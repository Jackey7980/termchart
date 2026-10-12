// adjacency_matrix.cpp —— 邻接矩阵实现
#include "termchart/graph/diagrams/adjacency_matrix.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::graph {
namespace {

// 值 → 块字符：相对最大值分三档（█ / ▓ / ░）。
char32_t value_char(double v, double vmax, char32_t zero) noexcept {
    if (!(v > 0.0) || !(vmax > 0.0)) {
        return zero;
    }
    const double t = v / vmax;
    if (t >= 2.0 / 3.0) {
        return U'\u2588';
    }
    if (t >= 1.0 / 3.0) {
        return U'\u2593';
    }
    return U'\u2591';
}

}  // namespace

Rect render_adjacency_matrix(const std::vector<std::string>& names,
                             const std::vector<std::vector<double>>& matrix,
                             canvas::Canvas& canvas, const AdjacencyMatrixOptions& opts) {
    const std::size_t n = names.size();
    if (n == 0) {
        return Rect{};
    }

    // 列宽：max(最小宽, 最宽列名)；行名列宽 = 最宽行名。
    const int col_w =
        std::max(opts.min_cell_width,
                 [&] {
                     int w = 1;
                     for (const std::string& name : names) {
                         w = std::max(w, util::display_width(name));
                     }
                     return w;
                 }());
    int label_w = 1;
    for (const std::string& name : names) {
        label_w = std::max(label_w, util::display_width(name));
    }

    // 全表最大值（分档基准）；全零表所有格子都画零值字符。
    double vmax = 0.0;
    for (std::size_t i = 0; i < n && i < matrix.size(); ++i) {
        for (std::size_t j = 0; j < n && j < matrix[i].size(); ++j) {
            if (std::isfinite(matrix[i][j])) {
                vmax = std::max(vmax, std::fabs(matrix[i][j]));
            }
        }
    }

    const double cell_step = static_cast<double>(col_w) + 1.0;
    const double table_x = static_cast<double>(label_w) + 1.0;

    // 1) 列名标尺（首行）。
    for (std::size_t j = 0; j < n; ++j) {
        const double x = table_x + static_cast<double>(j) * cell_step;
        canvas.draw_text(Point{x, 0.0}, names[j], opts.header_style);
    }

    // 2) 行名 + 格子。
    for (std::size_t i = 0; i < n; ++i) {
        const double y = static_cast<double>(i) + 1.0;
        canvas.draw_text(Point{0.0, y}, names[i], opts.label_style);
        for (std::size_t j = 0; j < n; ++j) {
            double v = 0.0;
            if (i < matrix.size() && j < matrix[i].size() && std::isfinite(matrix[i][j])) {
                v = std::fabs(matrix[i][j]);
            }
            const double x = table_x + static_cast<double>(j) * cell_step;
            canvas.put(Point{x, y}, value_char(v, vmax, opts.zero_char), opts.cell_style);
        }
    }

    const double total_w = table_x + static_cast<double>(n) * cell_step - 1.0;
    return Rect{0.0, 0.0, total_w, static_cast<double>(n) + 1.0};
}

}  // namespace termchart::graph
