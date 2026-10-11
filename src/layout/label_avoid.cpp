// label_avoid.cpp —— 标签避让实现
#include "termchart/layout/label_avoid.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace termchart::layout {
namespace {

// 半开区间相交：共享边界不算相交（相邻标签紧贴是允许的）。
bool overlaps(const Rect& a, const Rect& b) noexcept {
    return a.x < b.right() && b.x < a.right() && a.y < b.bottom() && b.y < a.bottom();
}

bool within_bounds(const Rect& candidate, const Rect& bounds) noexcept {
    if (bounds.w <= 0.0 || bounds.h <= 0.0) {
        return true;  // 未限制
    }
    return candidate.x >= bounds.x && candidate.y >= bounds.y &&
           candidate.right() <= bounds.right() && candidate.bottom() <= bounds.bottom();
}

bool is_free(const Rect& candidate, const std::vector<Rect>& occupied,
             const Rect& bounds) noexcept {
    // 越出左/上边界一律不采用：画布对负坐标是裁剪，硬放会让标签凭空消失。
    if (candidate.x < 0.0 || candidate.y < 0.0) {
        return false;
    }
    if (!within_bounds(candidate, bounds)) {
        return false;
    }
    for (const Rect& rect : occupied) {
        if (overlaps(candidate, rect)) {
            return false;
        }
    }
    return true;
}

Rect candidate_at(const LabelBox& label, double gap, int which) noexcept {
    const double w = static_cast<double>(label.width);
    const double h = static_cast<double>(label.height);
    switch (which) {
        case 0:  // 右
            return Rect{label.anchor.x + gap, label.anchor.y, w, h};
        case 1:  // 左
            return Rect{label.anchor.x - gap - w, label.anchor.y, w, h};
        case 2:  // 上
            return Rect{label.anchor.x, label.anchor.y - gap - h, w, h};
        default:  // 下
            return Rect{label.anchor.x, label.anchor.y + gap, w, h};
    }
}

}  // namespace

LabelSlot place_label_avoiding(const LabelBox& label, const std::vector<Rect>& occupied,
                               const LabelAvoidOptions& options) noexcept {
    const double gap = options.gap > 0.0 ? options.gap : 0.0;
    for (int which = 0; which < 4; ++which) {
        const Rect candidate = candidate_at(label, gap, which);
        if (is_free(candidate, occupied, options.bounds)) {
            return LabelSlot{candidate, true};
        }
    }
    return LabelSlot{candidate_at(label, gap, 0), false};
}

std::vector<LabelSlot> place_labels(const std::vector<LabelBox>& labels,
                                    std::vector<Rect> occupied,
                                    const LabelAvoidOptions& options) {
    std::vector<LabelSlot> slots;
    slots.reserve(labels.size());
    for (const LabelBox& label : labels) {
        const LabelSlot slot = place_label_avoiding(label, occupied, options);
        if (slot.placed) {
            occupied.push_back(slot.rect);
        }
        slots.push_back(slot);
    }
    return slots;
}

}  // namespace termchart::layout
