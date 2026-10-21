#include "termchart/data/rolling.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace termchart::data {

const char* rolling_stat_name(RollingStat s) noexcept {
    switch (s) {
    case RollingStat::Mean:   return "mean";
    case RollingStat::Sum:    return "sum";
    case RollingStat::Stddev: return "stddev";
    case RollingStat::Min:    return "min";
    case RollingStat::Max:    return "max";
    case RollingStat::Median: return "median";
    }
    return "?";
}

namespace {

double reduce_window(const double* data, std::size_t n, RollingStat stat) {
    if (n == 0) return std::nan("");
    if (stat == RollingStat::Sum) {
        double s = 0; for (std::size_t i = 0; i < n; ++i) s += data[i]; return s;
    }
    if (stat == RollingStat::Min) {
        double m = data[0]; for (std::size_t i = 1; i < n; ++i) if (data[i] < m) m = data[i]; return m;
    }
    if (stat == RollingStat::Max) {
        double m = data[0]; for (std::size_t i = 1; i < n; ++i) if (data[i] > m) m = data[i]; return m;
    }
    if (stat == RollingStat::Median) {
        std::vector<double> v(data, data + n);
        std::sort(v.begin(), v.end());
        if (n % 2 == 1) return v[n / 2];
        return 0.5 * (v[n / 2 - 1] + v[n / 2]);
    }
    double sum = 0;
    for (std::size_t i = 0; i < n; ++i) sum += data[i];
    const double mean = sum / static_cast<double>(n);
    if (stat == RollingStat::Mean) return mean;
    if (stat == RollingStat::Stddev) {
        double ss = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const double d = data[i] - mean;
            ss += d * d;
        }
        return std::sqrt(ss / static_cast<double>(n));
    }
    return std::nan("");
}

}  // namespace

std::vector<double>
rolling(const std::vector<double>& values, RollingStat stat,
        const RollingOptions& options) noexcept {
    std::vector<double> out;
    if (values.empty() || options.window == 0) return out;
    out.reserve(values.size());
    const std::size_t w = options.window;
    const std::size_t min_p = options.min_periods == 0 ? w : options.min_periods;
    for (std::size_t i = 0; i < values.size(); ++i) {
        std::size_t lo, hi;
        if (options.right_aligned) {
            lo = (i + 1 >= w) ? (i + 1 - w) : 0;
            hi = i + 1;
        } else {
            // 中心：左右各 w/2
            const std::size_t half = w / 2;
            lo = (i > half) ? (i - half) : 0;
            hi = std::min(values.size(), i + half + (w % 2 == 0 ? 1 : 1));
        }
        std::vector<double> window;
        window.reserve(hi - lo);
        for (std::size_t k = lo; k < hi; ++k) {
            if (!std::isnan(values[k])) window.push_back(values[k]);
        }
        if (window.size() < min_p) {
            out.push_back(std::nan(""));
        } else {
            out.push_back(reduce_window(window.data(), window.size(), stat));
        }
    }
    return out;
}

std::vector<double> ema(const std::vector<double>& values, std::size_t window) noexcept {
    std::vector<double> out;
    if (values.empty() || window == 0) return out;
    out.reserve(values.size());
    const double alpha = 2.0 / (static_cast<double>(window) + 1.0);
    bool seeded = false;
    double prev = 0.0;
    for (const double v : values) {
        if (std::isnan(v)) {
            out.push_back(std::nan(""));
            continue;
        }
        if (!seeded) {
            prev = v;
            seeded = true;
            out.push_back(prev);
            continue;
        }
        prev = alpha * v + (1.0 - alpha) * prev;
        out.push_back(prev);
    }
    return out;
}

}  // namespace termchart::data
