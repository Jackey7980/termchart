// tree_view.hpp —— 树形视图（缩进树）
//
// 字符化策略：一行一节点，├─ / └─ / │ 缩进前缀 + 名称文本（可选后缀值），
// 纯文本绘制走 draw_text，超出画布由裁剪兜底。布局自给（chart > graph 禁止边）。
#ifndef TERMCHART_CHART_HIERARCHICAL_TREE_VIEW_HPP
#define TERMCHART_CHART_HIERARCHICAL_TREE_VIEW_HPP

#include "termchart/chart/chart.hpp"
#include "termchart/chart/hierarchical/node.hpp"

namespace termchart::chart {

struct TreeViewOptions {
    bool show_values = false;  // 名称后附聚合值
};

class TreeView final : public Chart {
public:
    void set_root(hier::HierNode root);

    void draw(DrawContext ctx) override;
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    TreeViewOptions options;

private:
    hier::HierNode root_;
    bool has_root_ = false;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_HIERARCHICAL_TREE_VIEW_HPP
