// gradient.hpp —— 渐变生成
//
// 线性多段渐变：给色阶条、热度标尺、流式明暗用；真·连续色表走 colormap.hpp。
// 这里是无状态值类型：构造后只读，可被 style/theme 复用。
#ifndef TERMCHART_COLOR_GRADIENT_HPP
#define TERMCHART_COLOR_GRADIENT_HPP

#include <cstddef>
#include <vector>

#include "termchart/core/rgb.hpp"
#include "termchart/export.hpp"

namespace termchart::color {

// 插值空间。默认 Rgb 与历史行为逐位一致（golden 快照依赖它，不许动）；
// Hsv 让跨色相渐变（红→绿）不穿过中间的脏褐灰，代价是各色相段的感知速度不均。
enum class GradientMode {
    Rgb,  // sRGB 三分量直接线性插值
    Hsv,  // 色相走最短弧，饱和度 / 明度线性插值
};

class Gradient {
public:
    Gradient() = default;
    explicit Gradient(Rgb from, Rgb to) noexcept;

    // 切换插值空间。只影响之后调用 sample / generate 的结果。
    Gradient& set_mode(GradientMode mode) noexcept;

    // pos ∈ [0,1]；重复 pos 会覆盖；顺序无关（内部按 pos 排序）。
    Gradient& add_stop(double pos, Rgb color);
    Gradient& clear_stops() noexcept;

    [[nodiscard]] TERMCHART_API Rgb sample(double t) const noexcept;
    [[nodiscard]] TERMCHART_API std::vector<Rgb> generate(std::size_t count) const;
    [[nodiscard]] TERMCHART_API bool empty() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t stop_count() const noexcept;

private:
    struct Stop {
        double pos;
        Rgb color;
    };
    std::vector<Stop> stops_;
    GradientMode mode_ = GradientMode::Rgb;
};

// 两色之间线性插值，t 钳位到 [0,1]（sRGB 空间，不做 gamma 校正）。
TERMCHART_API Rgb mix(Rgb from, Rgb to, double t) noexcept;

// 指定插值空间。mode == Rgb 时与三参版本走同一条实现，输出逐位相同。
TERMCHART_API Rgb mix(Rgb from, Rgb to, double t, GradientMode mode) noexcept;

// 按感知亮度挑黑白之一作为前景，保证对比可读。
TERMCHART_API Rgb contrasting_text_color(Rgb background) noexcept;

}  // namespace termchart::color

#endif  // TERMCHART_COLOR_GRADIENT_HPP
