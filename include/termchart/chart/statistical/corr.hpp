// corr.hpp —— 相关矩阵（Pearson r → 字符色阶）
//
// 字符化策略：|r| 量化为 5 档映射到 '·'/'░'/'▒'/'▓'/'█'（阈值 0.15/0.40/
// 0.65/0.85），符号用前景色区分——正相关青、负相关红、近零暗灰；统计口径：
// Pearson 积差相关 r = cov/(σa·σb)，任一方差为 0（常数列）或长度不符的
// 部分按 r = 0（无相关）处理。
#ifndef TERMCHART_CHART_STATISTICAL_CORR_HPP
#define TERMCHART_CHART_STATISTICAL_CORR_HPP

#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

struct CorrMatrixOptions {
    double cell_w = 3.0;  // 每对变量横向占格（放不下时收缩，下限 1）
    double cell_h = 2.0;  // 纵向占格（放不下时收缩，下限 1）
};

class CorrMatrixChart final : public Chart {
public:
    // columns[j] 为第 j 个变量的观测序列；names 可为空，长度超过变量数时截断。
    TERMCHART_API void set_matrix(std::vector<std::string> names,
                                  std::vector<std::vector<double>> columns);

    void set_options(CorrMatrixOptions options) { options_ = options; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    // r[i][j] = 变量 i 与 j 的相关系数（对角线恒 1，退化格为 0）。
    [[nodiscard]] const std::vector<std::vector<double>>& r_matrix() const noexcept {
        return r_;
    }

private:
    // Pearson 积差相关；长度取两者较短者，零方差返回 0。
    static double pearson(const std::vector<double>& a,
                          const std::vector<double>& b) noexcept;

    std::vector<std::string> names_;
    std::vector<std::vector<double>> columns_;
    std::vector<std::vector<double>> r_;
    CorrMatrixOptions options_{};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_STATISTICAL_CORR_HPP
