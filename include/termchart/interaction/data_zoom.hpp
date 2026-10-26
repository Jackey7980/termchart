// data_zoom.hpp —— 数据轴范围滑块（DataZoom，纯几何 1D 组件）
//
// 终端图里"缩略窗口滑块"的本质：在数据轴 [data_min, data_max] 上开一个可见窗口
// [window_min, window_max]，并在一条屏幕"轨道"（Rect）上把它画成可拖拽的手柄盒。
// 用户拖左/右端点改窗口宽度、拖窗体内平移窗口；本类只吃几何量，不碰终端、不碰
// 渲染、不认识图表类型，依赖 L0 的 Point/Rect，与 zoom/pan/brush 同族——
// 可在无终端的单测里完整驱动，GUI/TUI 共用。
//
// 谁把窗口接到图上：调用方拿到 window_min()/window_max() 后，去改对应比例尺的
// domain（或 interaction::Viewport 的可见矩形）；本类不替你改任何下游状态。
//
// 不变量（构造后与每次状态变更后都维持）：
//   * 窗口始终完全落在数据界内：data_min <= window_min <= window_max <= data_max；
//   * 窗口跨度 >= min_window（数据量纲），且恒 <= 数据界跨度；
//   * 退化输入（数据界零跨度 / NaN·Inf / 轨道零长度）明确不改状态或取恒等值。
#ifndef TERMCHART_INTERACTION_DATA_ZOOM_HPP
#define TERMCHART_INTERACTION_DATA_ZOOM_HPP

#include <cstdint>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/export.hpp"

namespace termchart::interaction {

// 滑块沿哪个方向铺开：Horizontal 用轨道的 x..x+w，Vertical 用 y..y+h。
enum class ZoomAxis : std::uint8_t {
    Horizontal = 0,
    Vertical = 1,
};

struct DataZoomOptions {
    double min_window = 1e-9;  // 窗口允许的最小跨度（数据量纲）
    double handle_frac = 0.04; // 端点手柄半宽占轨道长度的比例（仅用于命中）
};

class DataZoom {
public:
    DataZoom() noexcept = default;
    DataZoom(double data_min, double data_max, double window_lo, double window_hi,
             Rect track, ZoomAxis axis = ZoomAxis::Horizontal,
             DataZoomOptions opts = {}) noexcept;

    void set_data_range(double lo, double hi) noexcept;
    void set_track(Rect track) noexcept;
    void set_axis(ZoomAxis axis) noexcept;
    void set_options(DataZoomOptions opts) noexcept;

    // 设置可见窗口；自动夹取进数据界并满足 lo<=hi 与最小跨度。
    void set_window(double lo, double hi) noexcept;
    // 窗口回到 == 全数据界。
    void reset() noexcept;

    // 轨道屏幕坐标 ↔ 数据值（沿 axis 方向；轨道外的坐标仍可折算，便于拖出界夹取）。
    [[nodiscard]] double value_at(double screen_pos) const noexcept;
    [[nodiscard]] double screen_pos_of(double value) const noexcept;

    enum class Grab { None, Left, Right, Body };

    // 命中：须落在轨道"带"内（Vertical 方向也在带宽里，避免误触）；
    // 端点手柄优先（屏幕像素半宽 handle_frac*轨道长），其次窗体内→Body，否则 None。
    [[nodiscard]] Grab hit_test(double screen_pos, double screen_other = 0.0) const noexcept;

    // 手势：begin 记录抓取部位；move 按部位改窗口；end 退出。
    void begin(double screen_pos) noexcept;
    void move(double screen_pos) noexcept;
    void end() noexcept;
    [[nodiscard]] bool dragging() const noexcept;

    [[nodiscard]] double data_min() const noexcept { return dmin_; }
    [[nodiscard]] double data_max() const noexcept { return dmax_; }
    [[nodiscard]] double window_min() const noexcept { return wlo_; }
    [[nodiscard]] double window_max() const noexcept { return whi_; }
    [[nodiscard]] Rect track() const noexcept { return track_; }
    [[nodiscard]] ZoomAxis axis() const noexcept { return axis_; }
    [[nodiscard]] const DataZoomOptions& options() const noexcept { return opts_; }

    // 窗口两端在轨道上的归一化位置 [0,1]，供绘制滑块手柄盒。
    [[nodiscard]] double window_lo_frac() const noexcept;
    [[nodiscard]] double window_hi_frac() const noexcept;

private:
    [[nodiscard]] double track_start() const noexcept;
    [[nodiscard]] double track_span() const noexcept;
    void clamp_window() noexcept;

    double dmin_ = 0.0;
    double dmax_ = 1.0;
    double wlo_ = 0.0;
    double whi_ = 1.0;
    Rect track_{};
    ZoomAxis axis_ = ZoomAxis::Horizontal;
    DataZoomOptions opts_{};
    Grab grab_ = Grab::None;
    bool dragging_ = false;
    double grab_offset_ = 0.0;  // Body 拖拽：指针初始位置相对窗口 lo 的偏移（数据量纲）
    double body_width_ = 0.0;   // Body 拖拽：按下时记录的窗口宽度
};

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_DATA_ZOOM_HPP
