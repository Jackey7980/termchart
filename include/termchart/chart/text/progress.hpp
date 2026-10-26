// progress.hpp —— 进度条组（横向条形，多值并列）
//
// **与 indicator/progress_ring 的区分**（规格注明，勿混）：本图是**横向
// 条形组**（多个 0..1 值并列，行式读数），progress_ring 是**单值环形**
// 读数。二者服务不同场景，不互相替代。
//
// 每行：标签 + 液条（█ 已完成 / ░ 未完成）+ 百分比。fraction 收敛到
// [0,1]，非有限值按 0 处理，不崩溃。
#ifndef TERMCHART_CHART_TEXT_PROGRESS_HPP
#define TERMCHART_CHART_TEXT_PROGRESS_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"

namespace termchart::chart {

struct ProgressOptions {
    std::size_t bar_width = 0;    // 0 = 自动：内容区宽 - 标签宽 - 百分比列
    bool show_percent = true;
};

class Progress final : public Chart {
public:
    void add_bar(std::string label, double fraction, Color color = default_color(DefaultColor::Accent));
    void set_options(const ProgressOptions& options) { options_ = options; }

    [[nodiscard]] std::size_t bar_count() const noexcept { return bars_.size(); }

    void draw(DrawContext ctx) override;

    // 真实 Scene 输出：标签 + 液条（█/░）+ 百分比，全部走 TextOp 字符落笔
    // （本图不用 Area2D，按单元格坐标复刻 draw()，几何整体归入 group 0）。
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

private:
    struct Bar {
        std::string label;
        double fraction = 0.0;
        Color color;
    };
    std::vector<Bar> bars_;
    ProgressOptions options_;
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_TEXT_PROGRESS_HPP
