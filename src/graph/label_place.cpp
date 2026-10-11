// label_place.cpp —— 标签避让实现
#include "termchart/graph/label_place.hpp"

#include <cstdint>
#include <vector>

namespace termchart::graph {

namespace {

// 半开区间相交判定：共享边界不算相交。
bool overlaps(const Rect& a, const Rect& b) noexcept {
    return a.x < b.right() && b.x < a.right() && a.y < b.bottom() && b.y < a.bottom();
}

bool is_free(const Rect& candidate, const std::vector<Rect>& occupied) noexcept {
    if (candidate.x < 0.0 || candidate.y < 0.0) {
        return false;  // 越出左/上边界：不采用（避免被画布裁剪或折回）
    }
    for (const Rect& rect : occupied) {
        if (overlaps(candidate, rect)) {
            return false;
        }
    }
    return true;
}

}  // namespace

LabelPlacement place_label(Point anchor, std::uint32_t width, std::uint32_t height,
                           const std::vector<Rect>& occupied) noexcept {
    const double w = static_cast<double>(width);
    const double h = static_cast<double>(height);

    const Rect right{anchor.x + 1.0, anchor.y, w, h};
    const Rect left{anchor.x - 1.0 - w, anchor.y, w, h};
    const Rect up{anchor.x, anchor.y - 1.0 - h, w, h};
    const Rect down{anchor.x, anchor.y + 1.0, w, h};

    for (const Rect& candidate : {right, left, up, down}) {
        if (is_free(candidate, occupied)) {
            return LabelPlacement{candidate, true};
        }
    }
    return LabelPlacement{right, false};
}

}  // namespace termchart::graph
