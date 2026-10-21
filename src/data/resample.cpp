#include "termchart/data/resample.hpp"

#include <algorithm>
#include <cmath>

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

    for (double v : values) {
        if (std::isnan(v)) continue;
        // 二分定位桶
        auto it = std::upper_bound(edges.begin(), edges.end(), v);
        std::size_t idx = static_cast<std::size_t>(it - edges.begin());
        if (idx == 0) continue;          // 小于首边界
        if (idx >= edges.size()) {
            // 越过末边界：归到末桶（最后一桶含 hi）
            if (v == edges.back()) idx = edges.size() - 1;
            else continue;
        }
        const std::size_t bucket = idx - 1;
        // 累加器：用均值类需要累加求和
        if (reducer == Reducer::Count) {
            out[bucket].count++;
        }
        // 把样本值存到 out[bucket].value 的 NaN 占位里：用 resample 的输出 value
        // 设计简化：分两次走（先聚合再规约）
    }

    // 第二遍：每个桶聚合（除非 Count）
    if (reducer == Reducer::Count) {
        for (auto& b : out) b.value = static_cast<double>(b.count);
        return out;
    }
    // 其他 reducer：累加 sum / 跟踪 min/max 等
    std::vector<double> sums(out.size(), 0.0);
    std::vector<double> mns(out.size(), 0.0);
    std::vector<double> mxs(out.size(), 0.0);
    std::vector<bool>   any(out.size(), false);
    std::vector<double> firsts(out.size(), 0.0);
    std::vector<double> lasts(out.size(), 0.0);
    for (double v : values) {
        if (std::isnan(v)) continue;
        auto it = std::upper_bound(edges.begin(), edges.end(), v);
        std::size_t idx = static_cast<std::size_t>(it - edges.begin());
        if (idx == 0 || idx >= edges.size()) continue;
        const std::size_t bucket = idx - 1;
        sums[bucket] += v;
        if (!any[bucket]) { mns[bucket] = v; mxs[bucket] = v; firsts[bucket] = v; }
        else { mxs[bucket] = std::max(mxs[bucket], v); mns[bucket] = std::min(mns[bucket], v); }
        any[bucket] = true;
        lasts[bucket] = v;
        out[bucket].count++;
    }
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (!any[i]) continue;
        switch (reducer) {
        case Reducer::Sum:    out[i].value = sums[i]; break;
        case Reducer::Mean:   out[i].value = sums[i] / static_cast<double>(out[i].count); break;
        case Reducer::Min:    out[i].value = mns[i]; break;
        case Reducer::Max:    out[i].value = mxs[i]; break;
        case Reducer::First:  out[i].value = firsts[i]; break;
        case Reducer::Last:   out[i].value = lasts[i]; break;
        case Reducer::Median:
        case Reducer::Stddev: {
            std::vector<double> bucket_vals;
            for (double v : values) {
                if (std::isnan(v)) continue;
                auto it = std::upper_bound(edges.begin(), edges.end(), v);
                std::size_t idx = static_cast<std::size_t>(it - edges.begin());
                if (idx == 0 || idx >= edges.size()) continue;
                if (idx - 1 == i) bucket_vals.push_back(v);
            }
            if (reducer == Reducer::Median) {
                std::sort(bucket_vals.begin(), bucket_vals.end());
                const std::size_t n = bucket_vals.size();
                if (n == 0) break;
                if (n % 2 == 1) out[i].value = bucket_vals[n / 2];
                else out[i].value = 0.5 * (bucket_vals[n / 2 - 1] + bucket_vals[n / 2]);
            } else {
                const double m = sums[i] / static_cast<double>(out[i].count);
                double ss = 0;
                for (double v : bucket_vals) { const double d = v - m; ss += d * d; }
                out[i].value = std::sqrt(ss / static_cast<double>(out[i].count));
            }
            break;
        }
        default: out[i].value = sums[i] / static_cast<double>(std::max<std::size_t>(out[i].count, 1)); break;
        }
    }
    return out;
}

}  // namespace termchart::data
