#include "termchart/util/data/aggregate.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <numeric>
#include <stdexcept>

namespace termchart::util::data {

const char* reducer_name(Reducer r) noexcept {
    switch (r) {
    case Reducer::Sum:     return "sum";
    case Reducer::Mean:    return "mean";
    case Reducer::Min:     return "min";
    case Reducer::Max:     return "max";
    case Reducer::Count:   return "count";
    case Reducer::First:   return "first";
    case Reducer::Last:    return "last";
    case Reducer::Median:  return "median";
    case Reducer::Stddev:  return "stddev";
    }
    return "?";
}

namespace {

struct Seg {
    std::size_t begin = 0;
    std::size_t end   = 0;  // 不含
};

double reduce_one(Reducer r, const double* data, std::size_t n) {
    if (n == 0) return std::nan("");
    if (r == Reducer::Count) return static_cast<double>(n);
    if (r == Reducer::First) return data[0];
    if (r == Reducer::Last)  return data[n - 1];

    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) sum += data[i];

    if (r == Reducer::Sum)  return sum;
    if (r == Reducer::Mean) return sum / static_cast<double>(n);

    double mn = data[0], mx = data[0];
    for (std::size_t i = 1; i < n; ++i) {
        if (data[i] < mn) mn = data[i];
        if (data[i] > mx) mx = data[i];
    }
    if (r == Reducer::Min) return mn;
    if (r == Reducer::Max) return mx;

    if (r == Reducer::Median) {
        std::vector<double> sorted(data, data + n);
        std::sort(sorted.begin(), sorted.end());
        if (n % 2 == 1) return sorted[n / 2];
        return 0.5 * (sorted[n / 2 - 1] + sorted[n / 2]);
    }

    if (r == Reducer::Stddev) {
        const double mean = sum / static_cast<double>(n);
        double ss = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double d = data[i] - mean;
            ss += d * d;
        }
        return std::sqrt(ss / static_cast<double>(n));
    }
    return std::nan("");
}

// 按 key 升序构造段（NaN key 跳过；相同 key 合并）
std::vector<std::pair<double, std::vector<Seg>>>
bucketize(DataView key_view, std::size_t num_cols,
          const std::vector<std::vector<std::size_t>>& col_indices_per_group) {
    std::map<double, std::vector<Seg>> buckets;
    const std::size_t n = std::min(key_view.size, num_cols > 0 ? key_view.size : 0);
    for (std::size_t i = 0; i < key_view.size; ++i) {
        const double k = key_view.data[i];
        if (std::isnan(k)) continue;
        buckets[k].push_back({i, i + 1});
    }
    // 合并相邻同 key 段
    std::vector<std::pair<double, std::vector<Seg>>> out;
    for (auto& [k, segs] : buckets) {
        std::sort(segs.begin(), segs.end(), [](const Seg& a, const Seg& b){
            return a.begin < b.begin;
        });
        std::vector<Seg> merged;
        for (auto& s : segs) {
            if (!merged.empty() && merged.back().end == s.begin) {
                merged.back().end = s.end;
            } else {
                merged.push_back(s);
            }
        }
        out.emplace_back(k, std::move(merged));
    }
    return out;
}

}  // namespace

std::vector<Group>
group_reduce(DataView key_view, std::vector<DataView> value_views, Reducer reducer) {
    std::vector<Group> out;
    if (key_view.data == nullptr || key_view.size == 0 || value_views.empty()) return out;

    auto buckets = bucketize(key_view, value_views.size(), {});
    out.reserve(buckets.size());
    for (auto& [k, segs] : buckets) {
        Group g;
        g.key = k;
        g.values_per_col.reserve(value_views.size());
        for (auto& v : value_views) {
            // 拼接所有段为一段 DataView（聚合 reducer 不关心顺序）
            std::vector<double> flat;
            flat.reserve(key_view.size);
            for (auto& s : segs) {
                for (std::size_t i = s.begin; i < s.end && i < v.size; ++i) {
                    flat.push_back(v.data[i]);
                }
            }
            g.values_per_col.push_back(DataView{flat.data(), flat.size()});
        }
        out.push_back(std::move(g));
    }
    return out;
}

MultiReduceResult
group_reduce_multi(DataView key_view,
                   std::vector<DataView> value_views,
                   std::vector<Reducer> reducers) {
    MultiReduceResult res;
    if (reducers.empty()) return res;
    res.reducers = reducers;
    auto buckets = bucketize(key_view, value_views.size(), {});
    res.groups.reserve(buckets.size());
    for (auto& [k, segs] : buckets) {
        Group g;
        g.key = k;
        std::vector<double> flat;
        for (auto& s : segs) {
            for (std::size_t i = s.begin; i < s.end; ++i) flat.push_back(key_view.data[i]);
        }
        g.values_per_col.push_back(DataView{flat.data(), flat.size()});
        res.groups.push_back(std::move(g));
    }
    return res;
}

std::vector<HistogramBin>
histogram(DataView view, std::size_t num_bins) {
    std::vector<HistogramBin> out;
    if (num_bins == 0 || view.data == nullptr || view.size == 0) return out;
    double mn = view.data[0], mx = view.data[0];
    for (std::size_t i = 1; i < view.size; ++i) {
        if (std::isnan(view.data[i])) continue;
        if (view.data[i] < mn) mn = view.data[i];
        if (view.data[i] > mx) mx = view.data[i];
    }
    if (mn == mx) {
        out.push_back({mn, mx, view.size});
        return out;
    }
    out.reserve(num_bins);
    const double w = (mx - mn) / static_cast<double>(num_bins);
    for (std::size_t b = 0; b < num_bins; ++b) {
        HistogramBin hb;
        hb.lo = mn + w * static_cast<double>(b);
        hb.hi = (b + 1 == num_bins) ? mx : (mn + w * static_cast<double>(b + 1));
        out.push_back(hb);
    }
    for (std::size_t i = 0; i < view.size; ++i) {
        const double v = view.data[i];
        if (std::isnan(v)) continue;
        std::size_t b = static_cast<std::size_t>((v - mn) / w);
        if (b >= num_bins) b = num_bins - 1;
        out[b].count++;
    }
    return out;
}

Quartiles quartiles(DataView view) {
    Quartiles q{};
    if (view.data == nullptr || view.size == 0) return q;
    std::vector<double> sorted;
    sorted.reserve(view.size);
    for (std::size_t i = 0; i < view.size; ++i) {
        if (!std::isnan(view.data[i])) sorted.push_back(view.data[i]);
    }
    if (sorted.empty()) return q;
    std::sort(sorted.begin(), sorted.end());
    auto pct = [&](double p) {
        const double pos = p * (sorted.size() - 1);
        const std::size_t lo = static_cast<std::size_t>(std::floor(pos));
        const std::size_t hi = static_cast<std::size_t>(std::ceil(pos));
        if (lo == hi) return sorted[lo];
        const double t = pos - static_cast<double>(lo);
        return sorted[lo] * (1 - t) + sorted[hi] * t;
    };
    q.minimum = sorted.front();
    q.maximum = sorted.back();
    q.q1      = pct(0.25);
    q.median  = pct(0.50);
    q.q3      = pct(0.75);
    return q;
}

}  // namespace termchart::util::data
