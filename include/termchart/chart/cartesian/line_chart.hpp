// line_chart.hpp —— 笛卡尔折线图
//
// 把若干 Series 画进 Canvas：网格 → 坐标轴+刻度 → 折线（含数据点标记）→
// 图例 → 标题。域可自动按数据范围 + 5% 留白 + nice 对齐（scale::nice_domain）。
#ifndef TERMCHART_CHART_CARTESIAN_LINE_CHART_HPP
#define TERMCHART_CHART_CARTESIAN_LINE_CHART_HPP

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "termchart/chart/chart.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/layout/box.hpp"
#include "termchart/layout/layout.hpp"
#include "termchart/scale/auto_domain.hpp"

namespace termchart::chart {

class LineChart final : public Chart {
public:
    // 追加一条系列（append 语义）。
    void add_series(Series s) { series_.push_back(std::move(s)); }
    // 替换全部系列（replace 语义）。**两个动词成对提供**：全库约定 `add_*` 是追加、
    // `set_*` 是替换，只给一半会逼调用方自己 clear()，而 clear() 并不总是对外可见。
    void set_series(std::vector<Series> series) { series_ = std::move(series); }
    [[nodiscard]] const std::vector<Series>& series() const noexcept { return series_; }

    void set_auto_domain(bool on, bool include_zero = false) {
        auto_domain_ = on;
        include_zero_ = include_zero;
    }

    // 显式 Y 域（权威值，优先于数据域）：给"轴域不该取极值"的场景用，如
    // autoviz 的离群裁剪（域外的点由比例尺钳到轴边缘，不丢系列连续性）。
    // 退化域（hi <= lo / 非有限）会被忽略——宁可退回自动域，也不画除零的轴。
    void set_y_domain(double lo, double hi) { y_domain_ = scale::Domain{lo, hi}; }
    [[nodiscard]] bool has_y_domain() const noexcept { return y_domain_.has_value(); }
    void set_show_legend(bool on) { show_legend_ = on; }

    // x 轴刻度标签格式化钩子：**只影响标签文本，不影响刻度位置**（位置仍由
    // scale::ticks_for 的 nice 值决定）。给时间轴用——x 是 epoch 秒这类连续
    // 数值、但人要看 "03-01" 而不是 "1772350000" 时，由调用方注入格式化，
    // chart 层不认识时间语义（与 layout 的字符串标签入口分层一致）。
    // 未设置时走 layout::format_tick 的默认数字格式。
    void set_x_tick_formatter(std::function<std::string(double)> formatter) {
        x_tick_formatter_ = std::move(formatter);
    }

    // 显式 x 刻度位置（替换 ticks_for 的自动 nice 值）。时间轴**必须**用它：
    // 在 epoch 秒上做 10 的幂取整会得到 50000 秒这种非人类间隔（= 13 小时 53 分），
    // 端点还会落到 02-28 17:33:20 这种假精度时刻上。chart 层仍不认识时间语义，
    // 由调用方（autoviz）按日历对齐算好刻度喂进来，与 set_x_tick_formatter 成对使用。
    // 传空 vector 恢复自动刻度。
    void set_x_ticks(std::vector<double> ticks) { x_ticks_ = std::move(ticks); }
    [[nodiscard]] bool has_x_ticks() const noexcept { return !x_ticks_.empty(); }
    // 只读访问（与 series() 同风格）：调用方/测试要核对"喂进来的刻度是什么"，
    // 不必渲染后从画布上反推文本。
    [[nodiscard]] const std::vector<double>& x_ticks() const noexcept { return x_ticks_; }

    void draw(DrawContext ctx) override;

    // 真实 Scene 输出：轴/刻度/图例/标题走 TextOp（它们本来就是字符艺术），
    // 折线走 MoveTo/LineTo + StrokeOp（矢量，比逐格 • 更连续），
    // 每条系列用 GroupOp 包起来供交互层成组命中。
    [[nodiscard]] bool to_scene(scene::Scene& out, std::uint32_t width,
                                std::uint32_t height) const override;

    // 组 i = 第 i 条非空系列（与 to_scene 的 begin_group 编号一致）。
    void animation_groups(std::vector<GroupProgress>& out) const override;

    // 暴露最后使用的绘图区，供测试断言几何。
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Series> series_;
    bool auto_domain_ = true;
    bool include_zero_ = false;
    bool show_legend_ = true;
    std::optional<scale::Domain> y_domain_;
    std::function<std::string(double)> x_tick_formatter_;
    std::vector<double> x_ticks_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

#endif  // TERMCHART_CHART_CARTESIAN_LINE_CHART_HPP
