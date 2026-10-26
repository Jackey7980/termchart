// dendrogram.hpp —— 树状图（叶均布底线、父节点横杠连接）
//
// 字符化策略：叶子沿底线均布并画 ●，父节点 x = 子节点均值、row = 深度，
// 连接件用 ─（横杠）与 │（垂线）拼 U 形。布局自给（chart > graph 禁止边）。
#ifndef TERMCHART_CHART_HIERARCHICAL_DENDROGRAM_HPP
#define TERMCHART_CHART_HIERARCHICAL_DENDROGRAM_HPP

#include "termchart/chart/chart.hpp"
#include "termchart/chart/hierarchical/node.hpp"

namespace termchart::chart {

struct DendrogramOptions {
    bool show_leaves = true;  // 叶子画 ● 标记
};

class Dendrogram final : public Chart {
public:
    void set_root(hier::HierNode root);

    void draw(DrawContext ctx) override;
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    DendrogramOptions options;

private:
    hier::HierNode root_;
    bool has_root_ = false;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_HIERARCHICAL_DENDROGRAM_HPP
