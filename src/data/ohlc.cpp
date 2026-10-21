#include "termchart/data/ohlc.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace termchart::data {

Result<std::vector<OhlcBar>> ohlc(std::span<const double> timestamps,
                                  std::span<const double> values,
                                  const OhlcOptions& options) noexcept {
    std::vector<OhlcBar> bars;
    const std::size_t n =
        timestamps.size() < values.size() ? timestamps.size() : values.size();
    if (n == 0) return Result<std::vector<OhlcBar>>{std::move(bars)};
    // 非降序是分桶算法的前提；违反即 fail-loud（不静默切错桶）。
    if (!std::is_sorted(timestamps.begin(), timestamps.end())) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }

    // 单桶模式（bucket_seconds ≤ 0）：全量一根。
    if (!(options.bucket_seconds > 0.0)) {
        OhlcBar bar;
        for (std::size_t i = 0; i < n; ++i) {
            if (std::isnan(values[i])) continue;
            if (bar.count == 0) {
                bar.t0 = timestamps[i];
                bar.open = values[i];
                bar.high = values[i];
                bar.low = values[i];
            } else {
                if (values[i] > bar.high) bar.high = values[i];
                if (values[i] < bar.low) bar.low = values[i];
            }
            bar.close = values[i];
            ++bar.count;
        }
        if (bar.count > 0) bars.push_back(bar);
        return Result<std::vector<OhlcBar>>{std::move(bars)};
    }

    // 分桶模式：桶边界锚定在桶内首个样本，顺序合并。
    OhlcBar cur;
    bool has = false;
    double cur_end = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        if (std::isnan(values[i])) continue;
        if (!has || timestamps[i] >= cur_end) {
            if (has) bars.push_back(cur);
            cur = OhlcBar{};
            cur.t0 = timestamps[i];
            cur.open = values[i];
            cur.high = values[i];
            cur.low = values[i];
            cur.close = values[i];
            cur.count = 1;
            cur_end = timestamps[i] + options.bucket_seconds;
            has = true;
            continue;
        }
        if (values[i] > cur.high) cur.high = values[i];
        if (values[i] < cur.low) cur.low = values[i];
        cur.close = values[i];
        ++cur.count;
    }
    if (has) bars.push_back(cur);
    return Result<std::vector<OhlcBar>>{std::move(bars)};
}

}  // namespace termchart::data
