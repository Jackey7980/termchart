// zoom.cpp —— 视口缩放实现（纯几何，无终端/渲染依赖）
#include "termchart/interaction/zoom.hpp"

#include <algorithm>
#include <cmath>

namespace termchart::interaction {

namespace {

// 把视口原点 lo 夹进 [data_lo, data_lo + data_span - span]。
// 当 data 方向退化（跨度非正）或可见跨度已占满数据界（span >= data_span）时，
// 该方向不可动，直接钉在 data_lo。
double clamp_origin(double lo, double data_lo, double data_span, double span) noexcept {
    if (!(data_span > 0.0) || !(span < data_span)) {
        return data_lo;
    }
    const double hi = data_lo + (data_span - span);
    if (lo < data_lo) {
        return data_lo;
    }
    if (lo > hi) {
        return hi;
    }
    return lo;
}

}  // namespace

Viewport::Viewport(const ViewportOptions& options) noexcept : options_(options) {
    // min_span 非有限或非正会破坏"可见跨度 >= min_span"这条不变量，回落到缺省正值。
    if (!(std::isfinite(options_.min_span) && options_.min_span > 0.0)) {
        options_.min_span = kDefaultMinSpan;
    }
    view_ = options_.data_bounds;
}

Viewport::Viewport(Rect data_bounds, double min_span, bool keep_aspect) noexcept
    : Viewport(ViewportOptions{data_bounds, min_span, keep_aspect}) {}

void Viewport::reset() noexcept { view_ = options_.data_bounds; }

void Viewport::zoom_at(double factor, Point anchor) noexcept {
    // 退化输入：factor 非有限（NaN/Inf）或 <= 0 —— 明确不改状态。
    if (!std::isfinite(factor) || factor <= 0.0) {
        return;
    }
    // data_bounds 有零/负跨度时无法定义缩放区间 —— 明确不改状态。
    if (options_.data_bounds.empty()) {
        return;
    }

    const double data_w = options_.data_bounds.w;
    const double data_h = options_.data_bounds.h;
    // 每轴允许的下界：data 界比 min_span 还窄时，以 data 界为准（否则会越界）。
    const double min_x = std::min(options_.min_span, data_w);
    const double min_y = std::min(options_.min_span, data_h);

    double span_x = 0.0;
    double span_y = 0.0;
    if (options_.keep_aspect) {
        // 两轴必须乘同一系数 k = 1/factor 才保持宽高比；把 k 夹进"两轴各自允许
        // 区间"的交集，于是夹取也不会破坏比例。
        const double k = 1.0 / factor;
        const double k_low = std::max(min_x / view_.w, min_y / view_.h);   // 放大上限
        const double k_high = std::min(data_w / view_.w, data_h / view_.h);  // 缩小上限
        // 约束互相冲突（min_span >= 某个 data 跨度）时以 data 界为准。
        const double k_use = (k_low > k_high) ? k_high : std::clamp(k, k_low, k_high);
        span_x = view_.w * k_use;
        span_y = view_.h * k_use;
    } else {
        // 两轴独立夹取：缩小不得越界（<= data 跨度），放大不得低于 min 下界。
        span_x = std::clamp(view_.w / factor, min_x, data_w);
        span_y = std::clamp(view_.h / factor, min_y, data_h);
    }

    // 锚点不动：new_lo = anchor - t * new_span，t 为 anchor 当前的归一化位置。
    // 随后夹进 data_bounds —— 若有夹取介入，贴边优先、锚点性质让步（见头文件说明）。
    const double tx = normalized_x(anchor.x);
    const double ty = normalized_y(anchor.y);
    const double lo_x =
        clamp_origin(anchor.x - tx * span_x, options_.data_bounds.x, data_w, span_x);
    const double lo_y =
        clamp_origin(anchor.y - ty * span_y, options_.data_bounds.y, data_h, span_y);

    view_ = Rect{lo_x, lo_y, span_x, span_y};
}

void Viewport::pan_by(double dx_data, double dy_data) noexcept {
    // 退化输入：平移量非有限 —— 明确不改状态。
    if (!std::isfinite(dx_data) || !std::isfinite(dy_data)) {
        return;
    }
    if (options_.data_bounds.empty()) {
        return;
    }
    // 逐轴夹取：越界贴边，不回绕。
    const double lo_x = clamp_origin(view_.x + dx_data, options_.data_bounds.x,
                                     options_.data_bounds.w, view_.w);
    const double lo_y = clamp_origin(view_.y + dy_data, options_.data_bounds.y,
                                     options_.data_bounds.h, view_.h);
    view_ = Rect{lo_x, lo_y, view_.w, view_.h};
}

Rect Viewport::visible() const noexcept { return view_; }

Rect Viewport::data_bounds() const noexcept { return options_.data_bounds; }

const ViewportOptions& Viewport::options() const noexcept { return options_; }

double Viewport::normalized_x(double data_x) const noexcept {
    return view_.w > 0.0 ? (data_x - view_.x) / view_.w : 0.5;
}

double Viewport::normalized_y(double data_y) const noexcept {
    return view_.h > 0.0 ? (data_y - view_.y) / view_.h : 0.5;
}

double Viewport::scale_x() const noexcept {
    return view_.w > 0.0 ? options_.data_bounds.w / view_.w : 1.0;
}

double Viewport::scale_y() const noexcept {
    return view_.h > 0.0 ? options_.data_bounds.h / view_.h : 1.0;
}

}  // namespace termchart::interaction
