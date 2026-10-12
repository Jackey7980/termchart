// sunburst.hpp —— 旭日图（径向环 + 角向扇区）
//
// 字符化策略：极坐标逐格采样——修正终端 1:2 宽高比后，环号 = 深度、
// 角向按聚合 value 比例分扇区，命中的格填 █。布局自给（同 treemap 的理由，
// chart > graph 是禁止边）。
#ifndef TERMCHART_CHART_HIERARCHICAL_SUNBURST_HPP
#define TERMCHART_CHART_HIERARCHICAL_SUNBURST_HPP

#include "termchart/chart/chart.hpp"
#include "termchart/chart/hierarchical/node.hpp"

namespace termchart::chart {

struct SunburstOptions {
    bool show_root = true;  // 最内环画根；关闭时子节点从内环起
};

class Sunburst final : public Chart {
public:
    void set_root(hier::HierNode root);

    void draw(DrawContext ctx) override;

    SunburstOptions options;

private:
    hier::HierNode root_;
    bool has_root_ = false;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_HIERARCHICAL_SUNBURST_HPP
