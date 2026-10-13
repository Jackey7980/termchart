// pca.hpp —— 主成分投影（协方差矩阵 + 幂迭代）
//
// 字符化策略：样本经 top-2 主成分投影后以 '●' 落格，轴即 PC1/PC2；统计
// 口径：列中心化后取协方差矩阵（除以 n），幂迭代求 PC1、对 C - λ1·v1·v1ᵀ
// 降幂求 PC2，解释方差比 = 特征值 / 总方差（迹）。投影退化（单点）时给
// 单位跨度；有效样本 <2 或维度 0 时不绘制。
#ifndef TERMCHART_CHART_STATISTICAL_PCA_HPP
#define TERMCHART_CHART_STATISTICAL_PCA_HPP

#include <cstddef>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/point.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"

namespace termchart::chart {

struct PcaOptions {
    int max_iter = 100;  // 幂迭代上限
    double tol = 1e-12;  // 收敛阈值（分量最大变化 / 归一化范数下限）
};

class PcaChart final : public Chart {
public:
    // rows：n 个样本，每个 d 维（以首行维度为准，长度不符或含非有限值的行剔除）。
    TERMCHART_API void set_data(std::vector<std::vector<double>> rows);

    void set_options(PcaOptions options) { options_ = options; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }
    // 投影后的 2D 点（与有效样本一一对应；未拟合时为空）。
    [[nodiscard]] const std::vector<Point>& projected() const noexcept { return projected_; }
    // [PC1, PC2] 的解释方差比（未拟合时为空）。
    [[nodiscard]] const std::vector<double>& explained_ratio() const noexcept {
        return ratio_;
    }

private:
    void fit();

    std::vector<std::vector<double>> rows_;
    PcaOptions options_{};
    std::vector<Point> projected_;
    std::vector<double> ratio_;
    Color color_ = default_color(DefaultColor::Neutral);
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_STATISTICAL_PCA_HPP
