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

    // 选项走 set_options()/options()，与其余图种一致（**不再**是裸 public 成员）：
    // 裸成员让"改了选项但没重画"成为合法的沉默错误，也让将来加不变量（例如
    // show_values 与 stage 数目的约束）无处可放。
    void set_options(const FunnelOptions& options) { options_ = options; }
    [[nodiscard]] const FunnelOptions& options() const noexcept { return options_; }

    void draw(DrawContext ctx) override;

private:
    std::vector<FunnelStage> stages_;
    FunnelOptions options_{};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_FLOW_FUNNEL_HPP
