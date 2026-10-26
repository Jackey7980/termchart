// icicle.hpp —— 冰柱图（每层一行、宽度 ∝ value 的嵌套分段）
//
// 字符化策略：深度 → 行（自上而下一层一行），子节点按 value 比例分割父节点
// 的水平区间；每段填 █，段宽 ≥2 时段首放名称首字符。布局自给（chart > graph
// 禁止边）。
#ifndef TERMCHART_CHART_HIERARCHICAL_ICICLE_HPP
#define TERMCHART_CHART_HIERARCHICAL_ICICLE_HPP

#include "termchart/chart/chart.hpp"
#include "termchart/chart/hierarchical/node.hpp"

namespace termchart::chart {

struct IcicleOptions {
    bool show_labels = true;  // 段首放名称首字符
};

class Icicle final : public Chart {
public:
    void set_root(hier::HierNode root);

    void draw(DrawContext ctx) override;
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    IcicleOptions options;

private:
    hier::HierNode root_;
    bool has_root_ = false;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_HIERARCHICAL_ICICLE_HPP
