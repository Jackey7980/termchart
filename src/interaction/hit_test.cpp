// hit_test.cpp —— 命中测试实现（纯几何）
#include "termchart/interaction/hit_test.hpp"

#include <cstdint>
#include <vector>

namespace termchart::interaction {

const chart::HitRegion* topmost_hit(const std::vector<chart::HitRegion>& regions,
                                    double x, double y) noexcept {
    // 逆序：后登记的在上层。
    for (auto it = regions.rbegin(); it != regions.rend(); ++it) {
        if (it->contains(x, y)) {
            return &(*it);
        }
    }
    return nullptr;
}

std::vector<std::uint32_t> hit_payloads(const std::vector<chart::HitRegion>& regions,
                                        double x, double y) {
    std::vector<std::uint32_t> payloads;
    for (const chart::HitRegion& region : regions) {
        if (region.contains(x, y)) {
            payloads.push_back(region.payload());
        }
    }
    return payloads;
}

}  // namespace termchart::interaction
