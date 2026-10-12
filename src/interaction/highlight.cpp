// highlight.cpp —— 悬停高亮 + 锚点放大实现（纯几何，无终端/渲染依赖）
#include "termchart/interaction/highlight.hpp"

#include <cmath>

namespace termchart::interaction {

Highlight::Highlight(HighlightOptions options) noexcept : options_(options) {}

void Highlight::set_hover(std::uint32_t payload, Point data_pos) noexcept {
    // 退化输入：非有限坐标是无效悬停，明确不改状态（旧悬停保持有效）。
    if (!std::isfinite(data_pos.x) || !std::isfinite(data_pos.y)) {
        return;
    }
    payload_ = payload;
    anchor_ = data_pos;
    active_ = true;
}

void Highlight::clear() noexcept {
    payload_ = 0;
    anchor_ = Point{};
    active_ = false;
}

bool Highlight::active() const noexcept { return active_; }

std::uint32_t Highlight::payload() const noexcept { return payload_; }

Point Highlight::anchor() const noexcept { return anchor_; }

const HighlightOptions& Highlight::options() const noexcept { return options_; }

Rect Highlight::magnified_around(Rect visible) const noexcept {
    // "<=1 或非有限视为不放大"：只影响本函数的取值，不改 options_。
    const double magnify = options_.magnify;
    if (!std::isfinite(magnify) || magnify <= 1.0) {
        return visible;
    }
    // 未悬停：没有放大中心，不做放大。
    if (!active_) {
        return visible;
    }
    // 空矩形没有"跨度"可除，原样返回。
    if (visible.empty()) {
        return visible;
    }
    const double span_x = visible.w / magnify;
    const double span_y = visible.h / magnify;
    // 锚点居中：new_lo = anchor - span/2。
    return Rect{anchor_.x - span_x / 2.0, anchor_.y - span_y / 2.0, span_x, span_y};
}

}  // namespace termchart::interaction
