// spatial_index.hpp —— 命中区域的空间哈希索引（"建一次、查多次"场景）
//
// 与 hit_test.hpp 的一次性线性扫描互补：当同一批 HitRegion 要被反复查询
// （GUI 悬停、鼠标连续移动、大散点图），先把区域装进 HitIndex，查询从 O(N)
// 降为近似 O(1)。现有 topmost_hit/hit_payloads 完全不改，调用方可按需 opt-in。
//
// 语义与 hit_test.hpp **逐位一致**：topmost 取最后登记者，payloads 自下而上。
// 索引内部持有 regions 的快照（不借用调用方 vector），因此不会因调用方
// 重建区域而悬垂；需要更新时显式 reset()。
#ifndef TERMCHART_INTERACTION_SPATIAL_INDEX_HPP
#define TERMCHART_INTERACTION_SPATIAL_INDEX_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/chart/hit_region.hpp"
#include "termchart/export.hpp"

namespace termchart::interaction {

class HitIndex {
public:
    HitIndex() = default;
    TERMCHART_API explicit HitIndex(const std::vector<chart::HitRegion>& regions);

    // 重建索引。传入空 vector 等价于清空；可安全重复调用。
    TERMCHART_API void reset(const std::vector<chart::HitRegion>& regions);

    [[nodiscard]] std::size_t size() const noexcept { return regions_.size(); }
    [[nodiscard]] bool empty() const noexcept { return regions_.empty(); }

    // 与 hit_test.hpp 同名同义；返回的指针指向索引内部快照。
    [[nodiscard]] TERMCHART_API const chart::HitRegion* topmost(double x, double y) const noexcept;
    [[nodiscard]] TERMCHART_API std::vector<std::uint32_t> payloads(double x, double y) const;

private:
    struct Grid {
        std::size_t cols = 0;
        std::size_t rows = 0;
        double min_x = 0.0;
        double min_y = 0.0;
        double cell_w = 1.0;
        double cell_h = 1.0;
    };

    // 调用方保证 x/y 有限；越界返回 nullptr（此时只剩 overflow_ 候选）。
    [[nodiscard]] const std::vector<std::uint32_t>* cell_at(double x, double y) const noexcept {
        if (grid_.cols == 0 || grid_.rows == 0) return nullptr;
        if (x < grid_.min_x || y < grid_.min_y) return nullptr;
        const auto cx = static_cast<std::size_t>((x - grid_.min_x) / grid_.cell_w);
        const auto cy = static_cast<std::size_t>((y - grid_.min_y) / grid_.cell_h);
        if (cx >= grid_.cols || cy >= grid_.rows) return nullptr;
        return &buckets_[cy * grid_.cols + cx];
    }

    Grid grid_{};
    std::vector<std::vector<std::uint32_t>> buckets_;  // 每格内的登记下标，升序
    std::vector<std::uint32_t> overflow_;              // 超大/退化区域，恒为候选，升序
    std::vector<chart::HitRegion> regions_;
};

}  // namespace termchart::interaction

#endif  // TERMCHART_INTERACTION_SPATIAL_INDEX_HPP
