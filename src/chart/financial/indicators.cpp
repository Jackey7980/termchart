// indicators.cpp —— 金融技术指标实现（纯数值，见 indicators.hpp）
#include "termchart/chart/financial/indicators.hpp"

#include <cmath>
#include <span>
#include <vector>

#include "termchart/math/stats.hpp"

namespace termchart::chart::financial {

namespace {
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
}  // namespace

std::vector<double> sma(std::span<const double> values, std::size_t window) {
    std::vector<double> out(values.size(), kNan);
    if (window == 0 || values.size() < window) {
        return out;
    }
    for (std::size_t i = window - 1; i < values.size(); ++i) {
        out[i] = math::mean(values.subspan(i - window + 1, window));
    }
    return out;
}

std::vector<double> ema(std::span<const double> values, std::size_t window) {
    std::vector<double> out(values.size(), kNan);
    if (values.empty() || window == 0) {
        return out;
    }
    const double alpha = 2.0 / (static_cast<double>(window) + 1.0);
    double prev = values[0];
    out[0] = prev;
    for (std::size_t i = 1; i < values.size(); ++i) {
        const double x = values[i];
        if (std::isnan(x)) {
            out[i] = prev;  // 非有限输入：保持已有 EMA 状态（与 data/rolling 口径一致）
        } else {
            prev = alpha * x + (1.0 - alpha) * prev;
            out[i] = prev;
        }
    }
    return out;
}

BollingerBands bollinger(std::span<const double> values, std::size_t window, double k) {
    BollingerBands bands;
    bands.mid = sma(values, window);
    bands.upper.assign(values.size(), kNan);
    bands.lower.assign(values.size(), kNan);
    if (values.size() < window) {
        return bands;
    }
    for (std::size_t i = window - 1; i < values.size(); ++i) {
        const double mid = bands.mid[i];
        const double sd = math::stddev(values.subspan(i - window + 1, window));
        if (std::isnan(sd)) {
            continue;
        }
        bands.upper[i] = mid + k * sd;
        bands.lower[i] = mid - k * sd;
    }
    return bands;
}

Macd macd(std::span<const double> values, std::size_t fast, std::size_t slow,
          std::size_t signal) {
    Macd m;
    m.macd.assign(values.size(), kNan);
    m.signal.assign(values.size(), kNan);
    m.hist.assign(values.size(), kNan);
    if (values.empty()) {
        return m;
    }
    const std::vector<double> ef = ema(values, fast);
    const std::vector<double> es = ema(values, slow);
    for (std::size_t i = 0; i < values.size(); ++i) {
        m.macd[i] = ef[i] - es[i];
    }
    const std::vector<double> sig = ema(std::span<const double>{m.macd}, signal);
    for (std::size_t i = 0; i < values.size(); ++i) {
        m.signal[i] = sig[i];
        m.hist[i] = m.macd[i] - sig[i];
    }
    return m;
}

}  // namespace termchart::chart::financial
