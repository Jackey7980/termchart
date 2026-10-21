// resample.cpp —— 重采样（按桶边界聚合）
//
// 2026-10-22 重写：旧实现遍历 values 最多三遍（Count 一遍、累加一遍、
// Median/Stddev **每桶一遍** = O(B×N)）。现单遍定位+收集，每桶一次规约：
// O(N + B·W log W)（Median 的 sort 只在桶内）。统计量复用 math::stats
// （Kahan/mean、nth_element/median、Welford/variance），比内联循环更准。
#include "termchart/data/resample.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

#include "termchart/math/stats.hpp"

namespace termchart::data {

std::vector<double>
linspace_edges(double lo, double hi, std::size_t n) {
    std::vector<double> edges;
    if (n < 2 || lo >= hi) return edges;
    edges.reserve(n);
    const double step = (hi - lo) / static_cast<double>(n - 1);
    for (std::size_t i = 0; i < n; ++i) {
        edges.push_back(lo + step * static_cast<double>(i));
    }
    edges.back() = hi;  // 防浮点漂移
    return edges;
}

std::vector<ResampleBin>
resample(const std::vector<double>& values, const std::vector<double>& edges,
         Reducer reducer) {
    std::vector<ResampleBin> out;
    if (edges.size() < 2) return out;
    out.resize(edges.size() - 1);
    for (std::size_t i = 0; i + 1 < edges.size(); ++i) {
        out[i].lo = edges[i];
        out[i].hi = edges[i + 1];
        out[i].value = std::nan("");
        out[i].count = 0;
    }

    // 单遍：每个样本二分定位桶号并落入该桶收集器。
    std::vector<std::vector<double>> bins(out.size());
    for (const double v : values) {
        if (std::isnan(v)) continue;
        const auto it = std::upper_bound(edges.begin(), edges.end(), v);
        std::size_t idx = static_cast<std::size_t>(it - edges.begin());
        if (idx == 0) continue;  // 小于首边界：不进任何桶
        if (idx >= edges.size()) {
            if (v == edges.back()) {
                idx = edges.size() - 1;  // 恰好等于末边界：归末桶（末桶含 hi）
            } else {
                continue;  // 越过末边界：丢弃
            }
        }
        bins[idx - 1].push_back(v);
    }

    // 每桶一次规约。桶内顺序 = 输入顺序（First/Last 依赖它）。
    for (std::size_t i = 0; i < out.size(); ++i) {
        const std::vector<double>& bv = bins[i];
        out[i].count = bv.size();
        if (bv.empty()) continue;  // value 保持 NaN（空桶口径）
        const std::span<const double> s{bv.data(), bv.size()};
        switch (reducer) {
        case Reducer::Sum:     out[i].value = math::sum(s); break;
        case Reducer::Mean:    out[i].value = math::mean(s); break;
        case Reducer::Min:     out[i].value = *std::min_element(bv.begin(), bv.end()); break;
        case Reducer::Max:     out[i].value = *std::max_element(bv.begin(), bv.end()); break;
        case Reducer::Count:   out[i].value = static_cast<double>(bv.size()); break;
        case Reducer::First:   out[i].value = bv.front(); break;
        case Reducer::Last:    out[i].value = bv.back(); break;
        case Reducer::Median:  out[i].value = math::median(s); break;
        case Reducer::Stddev:  out[i].value = std::sqrt(math::variance(s)); break;
        }
    }
    return out;
}

}  // namespace termchart::data
