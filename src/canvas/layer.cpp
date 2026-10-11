// layer.cpp —— 图层合成实现
#include "termchart/canvas/layer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "termchart/core/cell.hpp"

namespace termchart::canvas {

Layer::Layer(std::uint32_t width, std::uint32_t height) : canvas_(width, height) {}

void Layer::resize(std::uint32_t width, std::uint32_t height) {
    canvas_.resize(width, height);
    clean_ = CellBuffer{};
    has_clean_ = false;
}

void Layer::clear() {
    canvas_.clear();
}

CellBuffer Layer::to_buffer() const {
    return canvas_.to_buffer();
}

std::size_t Layer::composite_to(Canvas& target, Point at, Mode mode,
                                const Style* tint) const {
    // 落点取整：负值意味着左侧/上方被裁掉一部分。
    const auto offset_x = static_cast<std::int64_t>(std::floor(at.x));
    const auto offset_y = static_cast<std::int64_t>(std::floor(at.y));
    const auto target_w = static_cast<std::int64_t>(target.width());
    const auto target_h = static_cast<std::int64_t>(target.height());

    std::size_t written = 0;
    for (std::uint32_t y = 0; y < canvas_.height(); ++y) {
        const std::int64_t ty = offset_y + static_cast<std::int64_t>(y);
        if (ty < 0 || ty >= target_h) {
            continue;
        }
        for (std::uint32_t x = 0; x < canvas_.width(); ++x) {
            const std::int64_t tx = offset_x + static_cast<std::int64_t>(x);
            if (tx < 0 || tx >= target_w) {
                continue;
            }
            const Cell& cell = canvas_.at(x, y);
            if (mode != Mode::Replace && is_blank(cell)) {
                continue;  // Overlay/Tint 不写空白
            }
            const Style style = (mode == Mode::Tint && tint != nullptr) ? *tint
                                                                       : cell.style;
            target.put(static_cast<std::uint32_t>(tx), static_cast<std::uint32_t>(ty),
                       cell.cp, style);
            ++written;
        }
    }
    return written;
}

void Layer::mark_clean() {
    clean_ = canvas_.to_buffer();
    has_clean_ = true;
}

Rect Layer::dirty_bounds() const {
    if (!has_clean_ || clean_.width() != canvas_.width() ||
        clean_.height() != canvas_.height()) {
        return Rect{};  // 没有基线（或尺寸变了）：调用方应整层重绘
    }
    std::uint32_t min_x = canvas_.width();
    std::uint32_t min_y = canvas_.height();
    std::uint32_t max_x = 0;
    std::uint32_t max_y = 0;
    bool any = false;
    for (std::uint32_t y = 0; y < canvas_.height(); ++y) {
        for (std::uint32_t x = 0; x < canvas_.width(); ++x) {
            const Cell& now = canvas_.at(x, y);
            const Cell& before = clean_.at(x, y);
            const bool same = now.cp == before.cp && now.style == before.style;
            if (same) {
                continue;
            }
            any = true;
            min_x = std::min(min_x, x);
            min_y = std::min(min_y, y);
            max_x = std::max(max_x, x);
            max_y = std::max(max_y, y);
        }
    }
    if (!any) {
        return Rect{};  // 无差异
    }
    return Rect{static_cast<double>(min_x), static_cast<double>(min_y),
                static_cast<double>(max_x - min_x + 1),
                static_cast<double>(max_y - min_y + 1)};
}

}  // namespace termchart::canvas
