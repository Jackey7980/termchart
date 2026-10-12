// brush.cpp —— 刷选实现（纯几何，无终端/渲染依赖）
#include "termchart/interaction/brush.hpp"

namespace termchart::interaction {

Brush::Brush(BrushOptions options) noexcept : options_(options) {}

void Brush::set_items(const std::vector<std::pair<Point, std::uint32_t>>& items) noexcept {
    items_ = items;
}

void Brush::begin(Point p) noexcept {
    active_ = true;
    snap(p);
}

void Brush::stroke(Point p) noexcept {
    if (!active_) {
        return;  // 未落笔：不允许凭空"继续刷"。
    }
    snap(p);
}

void Brush::end() noexcept { active_ = false; }

void Brush::clear() noexcept {
    order_.clear();
    active_ = false;
}

bool Brush::active() const noexcept { return active_; }

std::vector<std::uint32_t> Brush::selected() const { return order_; }

bool Brush::selected(std::uint32_t payload) const noexcept {
    for (const std::uint32_t p : order_) {
        if (p == payload) {
            return true;
        }
    }
    return false;
}

const BrushOptions& Brush::options() const noexcept { return options_; }

void Brush::snap(Point p) noexcept {
    // 半径平方距离：radius 非有限/指针或点坐标非有限时比较为 false，
    // 天然退化为"不吸附"，无需特判（见头文件退化输入一节）。
    const double r2 = options_.radius * options_.radius;
    for (const auto& item : items_) {
        const double dx = p.x - item.first.x;
        const double dy = p.y - item.first.y;
        if (!(dx * dx + dy * dy <= r2)) {
            continue;
        }
        if (!selected(item.second)) {
            order_.push_back(item.second);
        }
    }
}

}  // namespace termchart::interaction
