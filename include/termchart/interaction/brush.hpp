// brush.hpp —— 刷选（数据点集的"笔刷式"吸附多选）
//
// 职责切分（为什么单开一个文件）：
//   * 只做"指针途经了哪些点"这一件事：给定 (数据坐标, payload) 点集，按吸附
//     半径把落笔/拖动途经的点收进选中集。不碰终端、不碰渲染、不认识图表类型，
//     只依赖 L0 的 Point —— 与 zoom/pan/select 同族的纯几何交互组件。
//   * "事件 → 手势"的路由（按下/拖动/松开映射到 begin/stroke/end）由上层或
//     immediate.hpp 完成；本类只吃几何量，可在无终端的单测里完整驱动。
//
// 手势模型（三段：落笔 → 途经 → 提笔）：
//   * begin(p)    进入刷选态，并按 p 吸附一次；
//   * stroke(p)   仅刷选态有效：途经吸附（未 begin 时是 no-op，防呆）；
//   * end()       退出刷选态；选中集**保留**供查询（像画笔提笔，画不擦）。
//
// 不变量（构造后与每次状态变更后都维持）：
//   * selected() 的顺序 = 各 payload 的**首次吸附顺序**，且无重复：同一
//     payload 对应的多个点、或同一点被反复途经，都只记一次；
//   * 选中集只增不减（begin 不清空上一笔——"刷"是累积语义，多笔画选多个
//     区域后统一读出），唯一的减少途径是 clear()；
//   * set_items() 随时可换点集，不改变当前选中集与手势状态（选中集按
//     payload 记录，与点集解耦；被移除点的 payload 仍留在选中集里）。
//
// 退化输入（明确不吸附、不改选中集；无需特判，比较语义天然给出）：
//   * radius 非有限（NaN/Inf）：半径平方为 NaN，所有距离比较为 false，
//     永不吸附；
//   * radius = 0：仅与点精确重合时吸附（精确拾取）；radius < 0：平方后与
//     |radius| 等价；
//   * 指针坐标或点坐标非有限：距离比较为 false，不吸附、不崩溃。
#ifndef TERMCHART_INTERACTION_BRUSH_HPP
#define TERMCHART_INTERACTION_BRUSH_HPP

#include <cstdint>
#include <utility>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/export.hpp"

namespace termchart::interaction {

struct BrushOptions {
    double radius = 1.5;  // 吸附半径（数据坐标量纲，由调用方决定）
};

class Brush {
public:
    explicit Brush(BrushOptions options = {}) noexcept;

    // 替换数据点集（内部拷贝一份）；不改变当前选中集与手势状态。
    void set_items(const std::vector<std::pair<Point, std::uint32_t>>& items) noexcept;

    // 落笔：进入刷选态，并按 p 吸附一次。已在刷选态再次 begin 等价于继续刷。
    void begin(Point p) noexcept;

    // 拖动途经：按 p 吸附（去重）。未 begin 时 no-op。
    void stroke(Point p) noexcept;

    // 提笔：退出刷选态，选中集保留。
    void end() noexcept;

    // 清空选中集并退出刷选态（唯一能减少选中集的入口）。
    void clear() noexcept;

    [[nodiscard]] bool active() const noexcept;  // begin 与 end 之间

    // 选中 payload 列表：按首次吸附顺序，去重。
    [[nodiscard]] std::vector<std::uint32_t> selected() const;

    // payload 是否在选中集里。
    [[nodiscard]] bool selected(std::uint32_t payload) const noexcept;

    [[nodiscard]] const BrushOptions& options() const noexcept;

private:
    // 把 p 半径内的点并入选中集（去重、保持首次顺序）。
    void snap(Point p) noexcept;

    BrushOptions options_{};
    std::vector<std::pair<Point, std::uint32_t>> items_;
    std::vector<std::uint32_t> order_;  // 首次吸附顺序
    bool active_ = false;
};

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_BRUSH_HPP
