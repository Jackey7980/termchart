#include "termchart/data/aggregate.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <numeric>
#include <stdexcept>

namespace termchart::data {

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

// 按 key 升序构造段（NaN key 跳过；相同 key 合并）
std::vector<std::pair<double, std::vector<Seg>>>
bucketize(DataView key_view, std::size_t num_cols,
          const std::vector<std::vector<std::size_t>>& col_indices_per_group) {
    std::map<double, std::vector<Seg>> buckets;
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
    // reducer 参数：保留在 API 面（pandas groupby().agg() 心智），当前只产出
    // 分组视图；聚合值由调用方对 values_per_col 施加 reducer 得到。
    (void)reducer;
    std::vector<Group> out;
    if (key_view.data == nullptr || key_view.size == 0 || value_views.empty()) return out;

    auto buckets = bucketize(key_view, value_views.size(), {});
    out.reserve(buckets.size());
    for (auto& [k, segs] : buckets) {
        Group g;
        g.key = k;
        g.values_per_col.reserve(value_views.size());
        g.owned.reserve(value_views.size());
        for (auto& v : value_views) {
            // 拼接所有段为一段（聚合 reducer 不关心顺序）。
            // 关键顺序：先搬进 owned（Group 成员，生命周期与 Group 一致），
            // 再从 owned 取指针——绝不能再"循环内临时 vector 存指针"。
            std::vector<double> flat;
            flat.reserve(key_view.size);
            for (auto& s : segs) {
                for (std::size_t i = s.begin; i < s.end && i < v.size; ++i) {
                    flat.push_back(v.data[i]);
                }
            }
            g.owned.push_back(std::move(flat));
            g.values_per_col.push_back(
                DataView{g.owned.back().data(), g.owned.back().size()});
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
    if (key_view.data == nullptr || key_view.size == 0 || value_views.empty()) return res;
    // 与 group_reduce 同构：按 key 分桶后，对**每个 value 列**收集段视图。
    // （历史 bug：此处曾把 key_view 当值列塞进 values_per_col、且忽略 value_views——
    // 那只会回显键，毫无意义；现修正为真正分组 value 列。）
    auto buckets = bucketize(key_view, value_views.size(), {});
    res.groups.reserve(buckets.size());
    for (auto& [k, segs] : buckets) {
        Group g;
        g.key = k;
        g.values_per_col.reserve(value_views.size());
        g.owned.reserve(value_views.size());
        for (auto& v : value_views) {
            std::vector<double> flat;
            flat.reserve(key_view.size);
            for (auto& s : segs) {
                for (std::size_t i = s.begin; i < s.end && i < v.size; ++i) {
                    flat.push_back(v.data[i]);
                }
            }
            // owned 持有，view 指向（勿再临时 vector 存指针）。
            g.owned.push_back(std::move(flat));
            g.values_per_col.push_back(
                DataView{g.owned.back().data(), g.owned.back().size()});
        }
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
        const double pos = p * static_cast<double>(sorted.size() - 1);
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

}  // namespace termchart::data
