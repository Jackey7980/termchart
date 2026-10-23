#include "termchart/data/fill_missing.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace termchart::data {

const char* fill_strategy_name(FillStrategy s) noexcept {
    switch (s) {
    case FillStrategy::ForwardFill:  return "forward_fill";
    case FillStrategy::LinearInterp: return "linear_interp";
    case FillStrategy::Drop:         return "drop";
    }
    return "?";
}

std::size_t
fill_missing(std::vector<double>& values, FillStrategy strategy) noexcept {
    std::size_t filled = 0;
    if (values.empty()) return 0;

    if (strategy == FillStrategy::ForwardFill) {
        double last = std::nan("");
        bool any = false;
        for (auto& v : values) {
            if (std::isnan(v)) {
                if (any) v = last;
                ++filled;
            } else {
                last = v;
                any = true;
            }
        }
        return filled;
    }

    if (strategy == FillStrategy::LinearInterp) {
        // 2026-10-22 重写为两遍 O(N)：旧实现对每个空洞向后扫描下一个非 NaN
        // （连续 k 个 NaN 时 O(k²)）。第一遍收集非 NaN 下标，第二遍在相邻
        // 锚点之间插值；首/尾空洞用最近锚点延拓（与旧口径一致）。
        if (values.empty()) return 0;
        std::vector<std::size_t> anchors;  // 非 NaN 下标，升序
        anchors.reserve(values.size());
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (!std::isnan(values[i])) anchors.push_back(i);
        }
        if (anchors.empty()) return 0;  // 全 NaN：无可填充

        std::size_t a = 0;  // 当前区间的左锚点在 anchors 中的位置
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (!std::isnan(values[i])) {
                // 推进左锚点：anchors[a] <= i。
                while (a + 1 < anchors.size() && anchors[a + 1] <= i) ++a;
                continue;
            }
            ++filled;
            if (i < anchors.front()) {
                values[i] = values[anchors.front()];  // 首锚点前：延拓
                continue;
            }
            if (i > anchors.back()) {
                values[i] = values[anchors.back()];   // 末锚点后：延拓
                continue;
            }
            // 区间 (anchors[a], anchors[a+1]) 内线性插值。
            const std::size_t left = anchors[a];
            const std::size_t right = anchors[a + 1];
            const double denom = static_cast<double>(right - left);
            const double t = static_cast<double>(i - left) / denom;
            values[i] = values[left] * (1.0 - t) + values[right] * t;
        }
        return filled;
    }

    return 0;
}

std::vector<double>
drop_missing(const std::vector<double>& values) {
    std::vector<double> out;
    out.reserve(values.size());
    for (double v : values) {
        if (!std::isnan(v)) out.push_back(v);
    }
    return out;
}

std::size_t
count_missing(const std::vector<double>& values) noexcept {
    std::size_t n = 0;
    for (double v : values) if (std::isnan(v)) ++n;
    return n;
}

}  // namespace termchart::data
