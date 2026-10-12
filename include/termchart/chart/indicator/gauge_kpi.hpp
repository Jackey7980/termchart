// gauge_kpi.hpp —— KPI 卡片：紧凑数字面板
//
// 卡片 = 边框 + 标签（上）+ 主值（中）+ 变化量（下，▲/▼）。
// 主值是**预格式化文本**——数字格式化策略（千分位、单位缩写）留给调用方，
// 库不替用户做决定。与 progress_ring 的分工：KPI 强调"读数"，不做弧形。
#ifndef TERMCHART_CHART_INDICATOR_GAUGE_KPI_HPP
#define TERMCHART_CHART_INDICATOR_GAUGE_KPI_HPP

#include <string>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

struct GaugeKpiOptions {
    bool show_delta = true;
};

class GaugeKpi final : public Chart {
public:
    void set_label(std::string text) { label_ = std::move(text); }
    void set_value(std::string text) { value_ = std::move(text); }
    void set_delta(std::string text, bool up) {
        delta_ = std::move(text);
        delta_up_ = up;
    }
    void set_options(const GaugeKpiOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    std::string label_;
    std::string value_;
    std::string delta_;
    bool delta_up_ = true;
    GaugeKpiOptions options_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_INDICATOR_GAUGE_KPI_HPP
