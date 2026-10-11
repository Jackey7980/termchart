// zoom.hpp —— 视口（数据坐标里的可见矩形）与缩放
//
// 职责切分（为什么单开一个文件）：
//   * 图表的"缩放"本质是"在数据坐标里换一个更小/更大的可见矩形"，这与终端、
//     渲染管线、具体图表类型统统无关，故只依赖 L0 的 Point/Rect，谁都能用。
//   * 平移（含拖拽手势）另见 pan.hpp；本文件只负责"视口 + 缩放"这一半。
//
// 不变量（构造后与每次状态变更后都维持）：
//   * visible() 始终完全落在 data_bounds 内 —— 不会露出界外空白；
//   * 任一方向的可见跨度 ∈ [min(min_span, data 该方向跨度), data 该方向跨度]。
//     即 data_bounds 比 min_span 还窄时以 data_bounds 为准（数据界优先于最小跨度）。
#ifndef TERMCHART_INTERACTION_ZOOM_HPP
#define TERMCHART_INTERACTION_ZOOM_HPP

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"

namespace termchart::interaction {

// min_span 的缺省正下界：量纲由调用方的数据坐标决定，这里只保证"大于 0"。
inline constexpr double kDefaultMinSpan = 1e-9;

struct ViewportOptions {
    Rect data_bounds{};  // 数据坐标里允许被观察的完整范围（夹取上限）
    double min_span = kDefaultMinSpan;  // 任一方向允许的最小可见跨度
    bool keep_aspect = false;           // 缩放时是否严格保持可见矩形的宽高比
};

// 数据坐标 → 屏幕/视口的可见窗口模型。纯值语义、纯函数式状态变换：
// 无隐藏状态、无随机、无时间/终端依赖，便于单测与 GUI 复用。
class Viewport {
public:
    Viewport() noexcept = default;
    explicit Viewport(const ViewportOptions& options) noexcept;
    Viewport(Rect data_bounds, double min_span = kDefaultMinSpan,
             bool keep_aspect = false) noexcept;

    // 回到初始态：可见矩形 == data_bounds。
    TERMCHART_API void reset() noexcept;

    // 以 anchor（数据坐标）为不动点做"乘法"缩放：
    //   factor > 1 放大（可见跨度 = 原跨度 / factor，看得更细），factor < 1 缩小。
    // 锚点性质：缩放前后 anchor 在视口内的归一化位置不变（夹取不介入时严格成立）。
    // 夹取策略：先按 factor 求出新跨度并在 [min_span, data 跨度] 内夹取（两轴在
    //   keep_aspect=false 时独立夹取；=true 时用同一系数联合夹取以锁死宽高比）；
    //   再由 anchor 反推原点并夹进 data_bounds。一旦夹取介入，**贴边优先**，
    //   锚点性质让步（此时 anchor 的归一化位置可能改变，这是有意的取舍）。
    // 退化输入：factor 非有限或 <= 0、或 data_bounds 有零/负跨度时，**明确不改状态**。
    TERMCHART_API void zoom_at(double factor, Point anchor) noexcept;

    // 把可见矩形在数据坐标里平移 (dx_data, dy_data)：正值 = 窗口向右/下移动
    // （屏幕上内容相对左移）。结果夹在 data_bounds 内，越界时贴边、不回绕。
    // 退化输入：平移量非有限、或 data_bounds 有零/负跨度时，**明确不改状态**。
    TERMCHART_API void pan_by(double dx_data, double dy_data) noexcept;

    [[nodiscard]] TERMCHART_API Rect visible() const noexcept;
    [[nodiscard]] TERMCHART_API Rect data_bounds() const noexcept;
    [[nodiscard]] TERMCHART_API const ViewportOptions& options() const noexcept;

    // 数据坐标 → 视口内归一化位置 [0,1]（视口外的点会越界，便于上层判断"在不在范围内"）。
    // 某方向可见跨度非正时该方向恒返回 0.5（无定义区间，取中点避免除零）。
    [[nodiscard]] TERMCHART_API double normalized_x(double data_x) const noexcept;
    [[nodiscard]] TERMCHART_API double normalized_y(double data_y) const noexcept;

    // 相对整个数据界的放大倍数 = data 跨度 / 可见跨度。reset 后为 1，放大后 > 1。
    // 可见跨度非正时返回 1（无定义，取恒等）。
    [[nodiscard]] TERMCHART_API double scale_x() const noexcept;
    [[nodiscard]] TERMCHART_API double scale_y() const noexcept;

private:
    ViewportOptions options_{};
    Rect view_{};
};

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_ZOOM_HPP
