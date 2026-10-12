// pack.hpp —— 圆堆积（bubble pack）
//
// 字符化策略：半径 ∝ √value 的圆，行式贪心排布（放不下换行）；圆周用 ●
// 描边、内部填 ·，半径足够时圆心放名称首字符。布局自给（chart > graph 禁止边）。
#ifndef TERMCHART_CHART_HIERARCHICAL_PACK_HPP
#define TERMCHART_CHART_HIERARCHICAL_PACK_HPP

#include "termchart/chart/chart.hpp"
#include "termchart/chart/hierarchical/node.hpp"

namespace termchart::chart {

struct PackOptions {
    double gap = 1.0;  // 气泡之间的空隙（格）
};

class PackChart final : public Chart {
public:
    void set_root(hier::HierNode root);

    void draw(DrawContext ctx) override;

    PackOptions options;

private:
    hier::HierNode root_;
    bool has_root_ = false;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_HIERARCHICAL_PACK_HPP
