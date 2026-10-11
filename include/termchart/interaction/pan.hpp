// pan.hpp —— 平移手势（纯几何拖拽状态机）
//
// 为什么与 zoom.hpp 分开：缩放是"换视口"的状态变换，拖拽是"把指针运动折算成位移"
// 的输入状态机，两者生命周期与依赖都不同（拖拽要记按下/释放）。
//
// 只吃几何量：本文件不碰终端、不碰渲染，也不做"终端格坐标 → 数据坐标"换算——
// 那步由上层用 Viewport 的映射函数完成。这样手势可在无终端的单测里完整驱动。
//
// 方向约定：拖拽采用"内容跟随指针"（像抓住地图拖动）。指针位移 (dx,dy) 对应视口
// 平移 (-dx,-dy)；drag_to() 返回**原始指针增量**，drag_into() 已封装这个取反。
#ifndef TERMCHART_INTERACTION_PAN_HPP
#define TERMCHART_INTERACTION_PAN_HPP

#include <optional>

#include "termchart/core/point.hpp"
#include "termchart/export.hpp"
#include "termchart/interaction/zoom.hpp"

namespace termchart::interaction {

// 按下 → 拖动 → 释放 的增量累积状态机。
class PanGesture {
public:
    // 按下：开始一次拖拽并记录起点。已在拖拽中再次按下会以新点重新开始。
    TERMCHART_API void press(Point at) noexcept;
    // 释放：结束拖拽。未在拖拽中调用是 no-op。
    TERMCHART_API void release() noexcept;
    [[nodiscard]] TERMCHART_API bool dragging() const noexcept;

    // 拖拽中：返回"自上次采样（或按下点）以来的**原始指针增量**"，并更新采样点。
    // 未拖拽：返回 std::nullopt —— 调用方必须忽略该次移动（视口保持不变）。
    [[nodiscard]] TERMCHART_API std::optional<Point> drag_to(Point at) noexcept;

    // 便捷：把本次拖拽按"内容跟随指针"折算后施加到视口（等价于 pan_by(-dx,-dy)）。
    // 返回是否消费了该次移动；未拖拽时返回 false 且**不动视口**。
    TERMCHART_API bool drag_into(Point at, Viewport& view) noexcept;

    [[nodiscard]] TERMCHART_API Point origin() const noexcept;   // 按下点
    [[nodiscard]] TERMCHART_API Point last() const noexcept;     // 最近采样点
    // 累计指针位移 = last - origin（各次增量之和），**不加到视口**，仅作状态查询。
    [[nodiscard]] TERMCHART_API Point pointer_displacement() const noexcept;

    // 结束拖拽并清空记录（不改变任何视口）。
    TERMCHART_API void reset() noexcept;

private:
    bool dragging_ = false;
    Point origin_{};
    Point last_{};
};

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_PAN_HPP
