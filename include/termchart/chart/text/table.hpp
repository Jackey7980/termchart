// table.hpp —— 数据表格
//
// 列宽 = 表头与该列所有单元格的最大**显示宽度**（util/wcwidth.hpp，CJK 宽
// 2 列）。总宽超出画布时整列裁掉（从右往左丢弃，至少保一列），单元格文本
// 超列宽按显示宽度截断（绝不切在组合记号中间）。表头加粗，表头下画一条
// ├─┤ 风格的分隔线。
#ifndef TERMCHART_CHART_TEXT_TABLE_HPP
#define TERMCHART_CHART_TEXT_TABLE_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

class Table final : public Chart {
public:
    void add_column(std::string name);
    // 不足列数的行尾部留空，超出列数的单元格忽略。
    void add_row(std::vector<std::string> cells);

    [[nodiscard]] std::size_t column_count() const noexcept { return columns_.size(); }
    [[nodiscard]] std::size_t row_count() const noexcept { return rows_.size(); }

    void draw(DrawContext ctx) override;

    // 真实 Scene 输出：表头 + 分隔线 + 单元格，全部走 TextOp 字符落笔
    // （本图不用 Area2D，按单元格坐标复刻 draw()，几何整体归入 group 0）。
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

private:
    std::vector<std::string> columns_;
    std::vector<std::vector<std::string>> rows_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_TEXT_TABLE_HPP
