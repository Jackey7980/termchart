// adjacency_matrix.hpp —— 邻接矩阵（行列标尺 + 值分级的块字符）
//
// 字符化策略：首行放列名标尺、每行行首放行名（均按显示宽度截断对齐），格子
// 按值相对最大值的比例分级为 █（强）▓（中）░（弱），零值画 ·——格粒度下
// "深浅"只有离散三档，不做假精度。
#ifndef TERMCHART_GRAPH_DIAGRAMS_ADJACENCY_MATRIX_HPP
#define TERMCHART_GRAPH_DIAGRAMS_ADJACENCY_MATRIX_HPP

#include <string>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct AdjacencyMatrixOptions {
    int min_cell_width = 3;  // 每列最小宽（列名按此对齐，超宽截断）
    char32_t zero_char = U'\u00B7';  // 零值字符（·）
    Style header_style{};
    Style label_style{};
    Style cell_style{};
};

// 就地绘制；返回整表包围盒。names 为空返回零矩形。
// matrix 行列不足 n 的部分按零值处理，多余的行列忽略。
[[nodiscard]] TERMCHART_API Rect render_adjacency_matrix(
    const std::vector<std::string>& names,
    const std::vector<std::vector<double>>& matrix, canvas::Canvas& canvas,
    const AdjacencyMatrixOptions& opts = {});

}  // namespace termchart::graph

#endif  // TERMCHART_GRAPH_DIAGRAMS_ADJACENCY_MATRIX_HPP
