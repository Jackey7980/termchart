// select.cpp —— 框选状态机实现（纯几何，无终端/渲染依赖）
#include "termchart/interaction/select.hpp"

#include <cmath>

namespace termchart::interaction {

void BoxSelect::begin(Point start) noexcept {
    dragging_ = true;
    start_ = start;
    current_ = start;
    // 重新起笔：已积累的矩形作废，直到下一次 update 才重新成立。
    has_rect_ = false;
}

void BoxSelect::update(Point current) noexcept {
    if (!dragging_) {
        return;  // 未按下：不允许凭空"继续拖"。
    }
    current_ = current;
    has_rect_ = true;
}

void BoxSelect::end() noexcept {
    if (!dragging_) {
        return;
    }
    dragging_ = false;  // 矩形保留：松开后仍要高亮框内点。
}

void BoxSelect::cancel() noexcept {
    dragging_ = false;
    has_rect_ = false;
}

bool BoxSelect::active() const noexcept { return dragging_; }

bool BoxSelect::has_rect() const noexcept { return has_rect_; }

Rect BoxSelect::rect() const noexcept {
    if (!has_rect_) {
        return Rect{};
    }
    const double lo_x = start_.x < current_.x ? start_.x : current_.x;
    const double lo_y = start_.y < current_.y ? start_.y : current_.y;
    // 归一化：任意方向拖拽都折算成"左上角 + 非负宽高"。
    return Rect{lo_x, lo_y, std::fabs(current_.x - start_.x), std::fabs(current_.y - start_.y)};
}

std::vector<std::uint32_t> BoxSelect::payloads_inside(
    const std::vector<std::pair<Point, std::uint32_t>>& items) const {
    std::vector<std::uint32_t> hits;
    if (!has_rect_) {
        return hits;
    }
    const Rect box = rect();
    for (const auto& [pos, payload] : items) {
        // 闭-开区间：含左/上边界，不含右/下边界，与 Rect::contains 一致。
        if (box.contains(pos)) {
            hits.push_back(payload);
        }
    }
    return hits;  // 按输入顺序，天然保序。
}

}  // namespace termchart::interaction
