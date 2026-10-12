// select.hpp —— 框选状态机（按下 → 拖动 → 松开/取消，纯几何，不吃 Event）
//
// 职责切分（为什么单开一个文件）：
//   * "拖出一个正矩形并回报框内数据点的 id"与终端、渲染管线、具体图表类型
//     统统无关，只依赖 L0 的 Point/Rect 与不透明 payload id。
//   * 本文件不直接消费 Event：由上层（immediate/调用方）把鼠标事件翻译成
//     begin/update/end/cancel 调用。
//
// 状态机（两态：空闲 / 拖拽中）：
//   * begin(start)      空闲 → 拖拽中；**拖拽中重复 begin 视为重新起笔**，
//                       起点重置、已积累的矩形作废；
//   * update(current)   仅拖拽中有效：更新当前点并使矩形可查询；
//                       空闲时是 **no-op**（防呆：不允许凭空"继续拖"）；
//   * end()             拖拽中 → 空闲；矩形**保留**供查询（松开后仍要高亮框内点）；
//                       空闲时是 no-op；
//   * cancel()          任何态 → 空闲并**清空矩形**（ESC 语义：什么都别选）。
//
// 不变量：
//   * has_rect() == true 当且仅当"拖拽中发生过至少一次 update"且此后未 cancel
//     （纯点击、只 begin 未拖动不算矩形——零面积矩形没有框选意义）；
//   * rect() 始终是归一化正矩形（x/y 为左上角、宽高非负），与 Rect 约定一致；
//   * active() == true 时 has_rect() 可以为 false（刚 begin 还没动）。
//
// 边界判定：payloads_inside 与 Rect::contains 的闭-开语义一致
//   （含左/上边界，不含右/下边界），从而与 HitRegion 的矩形命中可互推。
#ifndef TERMCHART_INTERACTION_SELECT_HPP
#define TERMCHART_INTERACTION_SELECT_HPP

#include <cstdint>
#include <utility>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"

namespace termchart::interaction {

class BoxSelect {
public:
    BoxSelect() noexcept = default;

    // 按下：进入拖拽态并记起点；拖拽中再次调用 = 重新起笔（重置起点、作废旧矩形）。
    TERMCHART_API void begin(Point start) noexcept;

    // 拖动：更新当前点。未 begin 时 no-op。
    TERMCHART_API void update(Point current) noexcept;

    // 松开：退出拖拽态，矩形保留供查询。未 begin 时 no-op。
    TERMCHART_API void end() noexcept;

    // ESC 语义：退出拖拽态并清空矩形。
    TERMCHART_API void cancel() noexcept;

    [[nodiscard]] TERMCHART_API bool active() const noexcept;    // 正在拖拽
    [[nodiscard]] TERMCHART_API bool has_rect() const noexcept;  // 松开后矩形仍可查询

    // 归一化正矩形（任意方向拖拽都返回左上角 + 非负宽高）；无矩形返回 {}。
    [[nodiscard]] TERMCHART_API Rect rect() const noexcept;

    // 框内数据点：items = (数据坐标, payload)；返回框内 payload，**保持输入顺序**。
    // 边界判定与 Rect::contains 的矩形语义一致（含左/上边界，不含右/下）。
    // 无矩形（has_rect() == false）时返回空。
    [[nodiscard]] TERMCHART_API std::vector<std::uint32_t> payloads_inside(
        const std::vector<std::pair<Point, std::uint32_t>>& items) const;

private:
    Point start_{};
    Point current_{};
    bool dragging_ = false;
    bool has_rect_ = false;
};

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_SELECT_HPP
