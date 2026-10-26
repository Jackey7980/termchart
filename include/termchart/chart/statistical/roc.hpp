// roc.hpp —— ROC/PR 曲线 + AUC
//
// 字符化策略：曲线逐点落 '•'，ROC 模式叠对角参考线 '·'（随机分类器基线）。
// 统计口径：按分数降序扫描阈值（同分合并为同一阈值），累积 (FPR, TPR) 得
// ROC、(recall, precision) 得 PR；AUC 用梯形积分。正类 = label > 0。正负
// 样本不同时缺（无法定义两指标）不绘制。
#ifndef TERMCHART_CHART_STATISTICAL_ROC_HPP
#define TERMCHART_CHART_STATISTICAL_ROC_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::chart {

enum class RocMode : std::uint8_t {
    Roc = 0,  // TPR 对 FPR
    Pr = 1,   // precision 对 recall
};

struct RocOptions {
    RocMode mode = RocMode::Roc;
    bool show_diagonal = true;  // 仅 ROC 模式生效
};

class RocChart final : public Chart {
public:
    // scores 与 labels 等长取较短者；label > 0 视为正类。非有限分数剔除。
    TERMCHART_API void set_scores(std::vector<double> scores,
                                  std::vector<int> labels,
                                  std::string name = "ROC");

    void set_options(RocOptions options) { options_ = options; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    // ROC/PR 曲线（数据坐标；正负类不全时为空）。
    [[nodiscard]] const std::vector<Point>& roc_curve() const noexcept { return roc_; }
    [[nodiscard]] const std::vector<Point>& pr_curve() const noexcept { return pr_; }
    [[nodiscard]] double auc_roc() const noexcept { return auc_roc_; }
    [[nodiscard]] double auc_pr() const noexcept { return auc_pr_; }
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> scores_;
    std::vector<int> labels_;
    std::string name_ = "ROC";
    RocOptions options_{};
    bool show_legend_ = true;
    std::vector<Point> roc_;
    std::vector<Point> pr_;
    double auc_roc_ = 0.0;
    double auc_pr_ = 0.0;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_STATISTICAL_ROC_HPP
