// mosaic.hpp —— 马赛克（marimekko）：列宽∝第一变量边际、列内高度∝第二变量占比。
//
// 终端字符化策略：列内自下而上按行类别堆叠 █ 色块，列边界画 │ 分隔，
// 边际与条件分布同时靠"宽"与"高"两条几何通道编码，无需坐标轴即可比较。
#ifndef TERMCHART_CHART_CARTESIAN_MOSAIC_HPP
#define TERMCHART_CHART_CARTESIAN_MOSAIC_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

class MosaicChart final : public Chart {
public:
    struct Options {
        bool show_legend = true;   // 行类别图例
        bool show_separators = true;
    };

    // counts[r][c]：行类别 r 在列类别 c 的计数；负值按 0 处理。
    TERMCHART_API void set_counts(std::vector<std::vector<double>> counts,
                                  std::vector<std::string> row_labels,
                                  std::vector<std::string> col_labels);
    void set_options(const Options& options) { options_ = options; }
    [[nodiscard]] const Options& options() const noexcept { return options_; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<std::vector<double>> counts_;
    std::vector<std::string> row_labels_;
    std::vector<std::string> col_labels_;
    Options options_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_MOSAIC_HPP
