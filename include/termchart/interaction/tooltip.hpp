// tooltip.hpp —— 悬停数据标签（带边框的小浮层）
//
// 职责：给定锚点格 + 文本，在画布上画一个带边框、可多行的浮层；并在 clear() 或
// 下一次 draw() 时**精确还原**被改写的格（否则悬浮一次就永久污染画布）。
//
// 定位规则（复用 layout/label_avoid 的既有思路/实现）：
//   * 候选顺序：右上 → 左上 → 右下 → 左下（角位，而非 label_avoid 的边位）；
//   * "可用"的判定**直接调用** layout::place_label_avoiding —— 把候选矩形反推成它
//     的"右"候选来做探测，因此重叠判定、gap、半开区间与越界规则完全复用，不另写一套；
//   * 锚点格恒计入占用集，故浮层**永不覆盖锚点**；
//   * 四角都不可用 → 返回 placed=false 且**不改画布**（对应"宽度不足以放任何浮层"
//     与"锚点在画布外"的退化输入），而不是硬塞。
//
// 宽度：按 util::display_width（宽字符算 2 列）计算；超过 max_width 时按显示宽度
// 截断并追加省略号（用 prefix_by_width，绝不切在组合记号中间），既有边框也不会被撑破。
#ifndef TERMCHART_INTERACTION_TOOLTIP_HPP
#define TERMCHART_INTERACTION_TOOLTIP_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/export.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::interaction {

struct TooltipOptions {
    Rect bounds{};                // 允许区域（通常 = 画布尺寸矩形）；w/h <= 0 表示该方向不限
    std::uint32_t max_width = 0;  // 内容最大显示宽（不含左右边框）；0 = 不限
    double gap = 1.0;             // 浮层与锚点之间的间距（格）
    char32_t border_h = U'─';
    char32_t border_v = U'│';
    char32_t corner_tl = U'┌';
    char32_t corner_tr = U'┐';
    char32_t corner_bl = U'└';
    char32_t corner_br = U'┘';
    char32_t ellipsis = U'…';
    Style box_style{};            // 内容与底板样式（浮层是不透明替换语义）
    Style border_style{};         // 边框样式
    util::WidthOptions width_options{};  // 宽度口径，与 util::display_width 一致
};

struct TooltipPlacement {
    Rect rect{};   // 浮层外框（含边框）的落点（格矩形）
    bool placed = false;
};

class Tooltip {
public:
    Tooltip() = default;
    explicit Tooltip(TooltipOptions options) noexcept;

    // 换配置。不主动碰画布：新配置在下一次 draw() 生效，而 draw() 会先还原旧浮层。
    TERMCHART_API void set_options(TooltipOptions options) noexcept;
    [[nodiscard]] TERMCHART_API const TooltipOptions& options() const noexcept;

    // 单行便捷：等价于 draw(canvas, anchor, {std::string(text)})。
    TERMCHART_API TooltipPlacement draw(canvas::Canvas& canvas, Point anchor,
                                        std::string_view text);
    // 多行：逐行按显示宽度截断（必要时加省略号），逐行补空格对齐右边框。
    // 退化（空行列表 / 行宽全 0 / 锚点在允许区域外 / 四角都放不下）→ placed=false
    // 且**不改画布**。重复调用先还原上一次浮层再重绘（幂等）。
    TERMCHART_API TooltipPlacement draw(canvas::Canvas& canvas, Point anchor,
                                        const std::vector<std::string>& lines);

    // 还原全部被改写的格；返回是否真的还原过。未绘制时为 no-op。
    TERMCHART_API bool clear(canvas::Canvas& canvas);

    [[nodiscard]] TERMCHART_API bool visible() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t patch_count() const noexcept;

private:
    struct Patch {
        std::uint32_t x = 0;
        std::uint32_t y = 0;
        Cell before{};
    };

    bool restore(canvas::Canvas& canvas);
    void capture(const canvas::Canvas& canvas, std::uint32_t x, std::uint32_t y);
    void reset_captured(std::uint32_t width, std::uint32_t height);

    TooltipOptions options_{};
    std::vector<Patch> patches_;
    std::vector<std::uint8_t> captured_;  // 尺寸 = captured_w_ * captured_h_
    std::uint32_t captured_w_ = 0;
    std::uint32_t captured_h_ = 0;
};

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_TOOLTIP_HPP
