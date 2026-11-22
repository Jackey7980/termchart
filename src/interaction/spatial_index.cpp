// spatial_index.cpp —— 空间哈希网格索引
#include "termchart/interaction/spatial_index.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace termchart::interaction {

namespace {

// 网格上限 128×128 = 16384 桶；单个区域覆盖超过 64 桶就改挂 overflow_，
// 避免一个铺满画布的背景区域把所有查询拖回 O(N)。
constexpr std::size_t kMaxGridSide = 128;
constexpr std::size_t kMaxCellsPerRegion = 64;

[[nodiscard]] bool finite_rect(const Rect& r) noexcept {
    return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.w) &&
           std::isfinite(r.h) && r.w > 0.0 && r.h > 0.0;
}

[[nodiscard]] std::size_t clamp_cell(double v, std::size_t count) noexcept {
    if (v <= 0.0) return 0;
    if (v >= static_cast<double>(count)) return count - 1;
    return static_cast<std::size_t>(v);
}

}  // namespace

HitIndex::HitIndex(const std::vector<chart::HitRegion>& regions) { reset(regions); }

void HitIndex::reset(const std::vector<chart::HitRegion>& regions) {
    regions_ = regions;
    buckets_.clear();
    overflow_.clear();
    grid_ = Grid{};

    const std::size_t n = regions_.size();
    if (n == 0) return;

    double min_x = std::numeric_limits<double>::infinity();
    double min_y = std::numeric_limits<double>::infinity();
    double max_x = -std::numeric_limits<double>::infinity();
    double max_y = -std::numeric_limits<double>::infinity();
    std::size_t valid = 0;
    for (const chart::HitRegion& region : regions_) {
        const Rect& b = region.bounds();
        if (!finite_rect(b)) continue;
        min_x = std::min(min_x, b.x);
        min_y = std::min(min_y, b.y);
        max_x = std::max(max_x, b.right());
        max_y = std::max(max_y, b.bottom());
        ++valid;
    }
    if (valid == 0) {
        for (std::size_t i = 0; i < n; ++i) overflow_.push_back(static_cast<std::uint32_t>(i));
        return;
    }

    const double span_x = max_x - min_x;
    const double span_y = max_y - min_y;
    const double area = span_x * span_y;
    if (!std::isfinite(span_x) || !std::isfinite(span_y) || !std::isfinite(area) || area <= 0.0) {
        for (std::size_t i = 0; i < n; ++i) overflow_.push_back(static_cast<std::uint32_t>(i));
        return;
    }

    // 目标：总桶数 ≈ 区域数，保持横纵比，再夹到网格上限。
    const double density = std::sqrt(static_cast<double>(n) / area);
    auto side = [](double v) -> std::size_t {
        const double rounded = std::ceil(v);
        if (!std::isfinite(rounded) || rounded < 1.0) return 1;
        return std::min(static_cast<std::size_t>(rounded), kMaxGridSide);
    };
    grid_.cols = side(span_x * density);
    grid_.rows = side(span_y * density);
    grid_.min_x = min_x;
    grid_.min_y = min_y;
    grid_.cell_w = span_x > 0.0 ? span_x / static_cast<double>(grid_.cols) : 1.0;
    grid_.cell_h = span_y > 0.0 ? span_y / static_cast<double>(grid_.rows) : 1.0;
    buckets_.assign(grid_.cols * grid_.rows, {});

    for (std::size_t i = 0; i < n; ++i) {
        const Rect& b = regions_[i].bounds();
        const std::uint32_t idx = static_cast<std::uint32_t>(i);
        if (!finite_rect(b)) {
            overflow_.push_back(idx);
            continue;
        }
        const double rx0 = (b.x - min_x) / grid_.cell_w;
        const double rx1 = (b.right() - min_x) / grid_.cell_w;
        const double ry0 = (b.y - min_y) / grid_.cell_h;
        const double ry1 = (b.bottom() - min_y) / grid_.cell_h;
        if (!std::isfinite(rx0) || !std::isfinite(rx1) || !std::isfinite(ry0) ||
            !std::isfinite(ry1)) {
            overflow_.push_back(idx);
            continue;
        }
        std::size_t cx0 = clamp_cell(rx0, grid_.cols);
        std::size_t cx1 = clamp_cell(rx1, grid_.cols);
        std::size_t cy0 = clamp_cell(ry0, grid_.rows);
        std::size_t cy1 = clamp_cell(ry1, grid_.rows);
        if (cx1 < cx0) std::swap(cx0, cx1);
        if (cy1 < cy0) std::swap(cy0, cy1);
        if ((cx1 - cx0 + 1) * (cy1 - cy0 + 1) > kMaxCellsPerRegion) {
            overflow_.push_back(idx);
            continue;
        }
        for (std::size_t cy = cy0; cy <= cy1; ++cy) {
            for (std::size_t cx = cx0; cx <= cx1; ++cx) {
                buckets_[cy * grid_.cols + cx].push_back(idx);
            }
        }
    }
}

const chart::HitRegion* HitIndex::topmost(double x, double y) const noexcept {
    // NaN/Inf 查询走与 hit_test.hpp 完全相同的线性语义，避免索引短路改变结果。
    if (!std::isfinite(x) || !std::isfinite(y)) {
        for (auto it = regions_.rbegin(); it != regions_.rend(); ++it) {
            if (it->contains(x, y)) return &(*it);
        }
        return nullptr;
    }

    const std::vector<std::uint32_t>* const cell = cell_at(x, y);
    std::size_t oi = overflow_.size();
    std::size_t ci = cell != nullptr ? cell->size() : 0;
    while (oi > 0 || ci > 0) {
        std::uint32_t idx = 0;
        if (ci > 0 && (oi == 0 || (*cell)[ci - 1] >= overflow_[oi - 1])) {
            idx = (*cell)[--ci];  // 双有序表逆序归并：先取登记号大的
        } else {
            idx = overflow_[--oi];
        }
        if (regions_[idx].contains(x, y)) return &regions_[idx];
    }
    return nullptr;
}

std::vector<std::uint32_t> HitIndex::payloads(double x, double y) const {
    std::vector<std::uint32_t> out;
    if (!std::isfinite(x) || !std::isfinite(y)) {
        for (const chart::HitRegion& region : regions_) {
            if (region.contains(x, y)) out.push_back(region.payload());
        }
        return out;
    }

    const std::vector<std::uint32_t>* const cell = cell_at(x, y);
    std::size_t oi = 0;
    std::size_t ci = 0;
    const std::size_t cn = cell != nullptr ? cell->size() : 0;
    const std::size_t on = overflow_.size();
    while (oi < on || ci < cn) {
        std::uint32_t idx = 0;
        if (ci < cn && (oi >= on || (*cell)[ci] <= overflow_[oi])) {
            idx = (*cell)[ci++];  // 正序归并：保持自下而上的登记顺序
        } else {
            idx = overflow_[oi++];
        }
        if (regions_[idx].contains(x, y)) out.push_back(regions_[idx].payload());
    }
    return out;
}

}  // namespace termchart::interaction
