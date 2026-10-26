// treemap.hpp —— 树图（矩形切片）
//
// 字符化策略：slice-and-dice 递归切片，面积 ∝ 聚合 value（沿较长边切分，
// 让格子尽量偏方）；叶子格块填 █，块足够大时在左上角放名称首字符。
// 布局自给：chart 是 L12、graph 是 L13 且 `chart > graph` 为禁止边，
// 层次布局由本文件内 ~30 行递归完成，不 include graph/。
#ifndef TERMCHART_CHART_HIERARCHICAL_TREEMAP_HPP
#define TERMCHART_CHART_HIERARCHICAL_TREEMAP_HPP

#include "termchart/chart/chart.hpp"
#include "termchart/chart/hierarchical/node.hpp"

namespace termchart::chart {

struct TreemapOptions {
    bool show_labels = true;  // 叶子左上角放名称首字符
};

class Treemap final : public Chart {
public:
    void set_root(hier::HierNode root);

    void draw(DrawContext ctx) override;
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    TreemapOptions options;

private:
    hier::HierNode root_;
    bool has_root_ = false;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_HIERARCHICAL_TREEMAP_HPP
