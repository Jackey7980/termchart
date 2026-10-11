#include "termchart/core/grapheme.hpp"

#include "termchart/core/cell.hpp"

namespace termchart {

bool GraphemeCluster::add_side_bearing(char32_t cp) noexcept {
    if (count_ >= sides_.size()) {
        return false;  // 超限：丢弃尾部，保证 CellBuffer 侧面结构体定长。
    }
    sides_[count_] = cp;
    ++count_;
    return true;
}

bool GraphemeCluster::operator==(const GraphemeCluster& other) const noexcept {
    if (base_ != other.base_ || count_ != other.count_) {
        return false;
    }
    for (std::size_t i = 0; i < count_; ++i) {
        if (sides_[i] != other.sides_[i]) {
            return false;
        }
    }
    return true;
}

bool GraphemeCluster::operator!=(const GraphemeCluster& other) const noexcept {
    return !(*this == other);
}

}  // namespace termchart
