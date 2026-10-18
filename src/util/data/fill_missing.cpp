#include "termchart/util/data/fill_missing.hpp"

#include <cmath>

namespace termchart::util::data {

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
        // 第一/最后一个非 NaN 值
        std::size_t first_idx = values.size(), last_idx = values.size();
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (!std::isnan(values[i])) { first_idx = i; break; }
        }
        for (std::size_t i = values.size(); i > 0; --i) {
            if (!std::isnan(values[i - 1])) { last_idx = i - 1; break; }
        }
        if (first_idx >= values.size()) return 0;  // 全 NaN
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (!std::isnan(values[i])) continue;
            if (i < first_idx)      values[i] = values[first_idx];
            else if (i > last_idx)  values[i] = values[last_idx];
            else {
                // 找 i 之前最近非 NaN（值在 prev-1 处），i 之后最近非 NaN（值在 next+1 处）。
                std::size_t prev = i;
                while (prev > 0 && std::isnan(values[prev - 1])) --prev;
                std::size_t next = i;
                while (next + 1 < values.size() && std::isnan(values[next + 1])) ++next;
                const double v_prev = values[prev - 1];
                const double v_next = values[next + 1];
                const double denom = static_cast<double>(next + 1) - static_cast<double>(prev - 1);
                const double t = denom > 0.0
                    ? (static_cast<double>(i) - static_cast<double>(prev - 1)) / denom
                    : 0.5;
                values[i] = v_prev * (1.0 - t) + v_next * t;
            }
            ++filled;
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

}  // namespace termchart::util::data
