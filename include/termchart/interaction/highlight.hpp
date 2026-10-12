// highlight.hpp —— 悬停高亮 + 锚点放大（纯状态，不吃 Event）
//
// 职责切分（为什么单开一个文件）：
//   * "悬停到哪个数据点 + 以该点为中心看多大范围"是纯几何/纯状态问题，与终端、
//     渲染管线、具体图表类型统统无关，只依赖 L0 的 Point/Rect。
//   * 本文件不直接消费 Event：由上层（immediate/调用方）把鼠标事件翻译成
//     set_hover/clear 调用。payload 是**不透明 id**（同 chart::HitRegion 的
//     契约），本文件只原样存取，不解释含义。
//
// 不变量（每次状态变更后都维持）：
//   * active() == true 当且仅当最近一次 set_hover 之后未 clear；
//   * active() == false 时 payload() 恒为 0（悬停无对象即无 id）；
//   * options().magnify **原样保存**，不归一化——"<=1 或非有限视为不放大"
//     是 magnified_around 的判定规则，不是存储规则。
//
// 退化输入（明确不改状态）：
//   * set_hover 传入非有限坐标：视为无效悬停，**不改任何状态**
//     （此前的悬停保持有效，不会因一条坏事件被清掉）；
//   * magnified_around：magnify <= 1 / 非有限、visible 为空矩形、或当前未
//     悬停（无锚点即无放大中心）时，**原样返回 visible**。
#ifndef TERMCHART_INTERACTION_HIGHLIGHT_HPP
#define TERMCHART_INTERACTION_HIGHLIGHT_HPP

#include <cstdint>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"

namespace termchart::interaction {

struct HighlightOptions {
    // 放大倍数；<=1 或非有限视为不放大。
    double magnify = 2.0;
};

class Highlight {
public:
    explicit Highlight(HighlightOptions options = {}) noexcept;

    // 进入/移动悬停：payload 原样保存（0 也合法，由调用方自行约定含义）。
    TERMCHART_API void set_hover(std::uint32_t payload, Point data_pos) noexcept;

    // 离开悬停：回到未激活态，payload 归零。
    TERMCHART_API void clear() noexcept;

    [[nodiscard]] TERMCHART_API bool active() const noexcept;
    [[nodiscard]] TERMCHART_API std::uint32_t payload() const noexcept;
    [[nodiscard]] TERMCHART_API Point anchor() const noexcept;
    [[nodiscard]] TERMCHART_API const HighlightOptions& options() const noexcept;

    // 高亮放大的核心：以锚点为中心的可见矩形，跨度 = 原跨度 / magnify，
    // 锚点保持居中（锚点性质：缩放前后 anchor 在矩形内的归一化位置不变，
    // 严格为中心 0.5）。不夹取——本函数不知道 data_bounds，夹取是调用方的事。
    // magnify <= 1 / 非有限、visible 为空、或未悬停时**原样返回 visible**。
    [[nodiscard]] TERMCHART_API Rect magnified_around(Rect visible) const noexcept;

private:
    HighlightOptions options_{};
    Point anchor_{};
    std::uint32_t payload_ = 0;
    bool active_ = false;
};

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_HIGHLIGHT_HPP
