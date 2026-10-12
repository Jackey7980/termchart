// funnel.hpp —— 漏斗图（逐段居中横条）
//
// 字符化策略：每个阶段占一行，条宽 ∝ value / 最大值、水平居中，填 █；
// 行首画名称文本（可选附值）。无轴，纯条 + 文本。
#ifndef TERMCHART_CHART_FLOW_FUNNEL_HPP
#define TERMCHART_CHART_FLOW_FUNNEL_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"

namespace termchart::chart {

struct FunnelStage {
    std::string name;
    double value = 0.0;
    Color color = Color::none();  // none → 按 stage 序号自动配色
};

struct FunnelOptions {
    bool show_values = true;  // 名称后附数值
};

class FunnelChart final : public Chart {
public:
    void set_stages(std::vector<FunnelStage> stages);

    void draw(DrawContext ctx) override;

    FunnelOptions options;

private:
    std::vector<FunnelStage> stages_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_FLOW_FUNNEL_HPP
