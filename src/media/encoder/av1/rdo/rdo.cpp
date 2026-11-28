#include "rdo.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "termchart/media/encoder/gpa/metrics.hpp"

namespace termchart::image::codec::av1 {

double av1_rdo_symbol_bits(const std::uint16_t* cdf, int n_symbols, int symbol) {
    if (symbol < 0 || symbol >= n_symbols) return 24.0;
    const int lo = symbol ? cdf[symbol - 1] : 0;
    const int hi = cdf[symbol];
    const int count = std::max(1, hi - lo);
    return -std::log2(static_cast<double>(count) / 32768.0);
}

double av1_rdo_residual_bits(const std::int16_t* coeff, int n) {
    double bits = 1.0;  // eob 标志
    for (int i = 0; i < n; ++i) {
        const int v = coeff[i] < 0 ? -coeff[i] : coeff[i];
        if (!v) continue;
        bits += 2.0;  // 非零 + 符号
        int magnitude = v;
        while (magnitude) { bits += 1.0; magnitude >>= 1; }
    }
    return bits;
}

std::uint64_t av1_rdo_sse(const std::uint8_t* a, const std::uint8_t* b, int n) {
    if (n <= 0) return 0;
    return termchart::media::encoder::gpa::ssd(a, b, static_cast<std::size_t>(n));
}

RdoModeResult av1_rdo_pick_mode(const std::uint8_t* src,
                                const std::uint8_t* candidates,
                                int n_candidates, const std::uint16_t* mode_cdf,
                                double lambda, bool early_exit) {
    RdoModeResult best;
    best.cost = std::numeric_limits<double>::infinity();
    for (int k = 0; k < n_candidates; ++k) {
        const std::uint8_t* pred = candidates + static_cast<std::size_t>(k) * 64;
        const double rate = mode_cdf ? av1_rdo_symbol_bits(mode_cdf, n_candidates, k)
                                     : 0.0;
        if (rate * lambda >= best.cost) continue;  // 码率下界剪枝
        double dist = 0;
        bool pruned = false;
        for (int i = 0; i < 64; ++i) {
            const int d = static_cast<int>(src[i]) - pred[i];
            dist += static_cast<double>(d * d);
            if (early_exit && dist * lambda + rate * lambda >= best.cost) {
                pruned = true;
                break;
            }
        }
        if (pruned) continue;
        const double cost = dist + lambda * rate;
        if (cost < best.cost) {
            best = RdoModeResult{k, static_cast<std::uint64_t>(dist), rate, cost};
        }
    }
    return best;
}

bool av1_rdo_should_split(const std::uint8_t* src, const std::uint8_t* whole,
                          const std::uint8_t* parts, int n, double lambda) {
    const double whole_cost =
        static_cast<double>(av1_rdo_sse(src, whole, n)) + lambda * 1.0;
    const double split_cost =
        static_cast<double>(av1_rdo_sse(src, parts, n)) + lambda * 4.0;
    return split_cost < whole_cost;
}

}  // namespace termchart::image::codec::av1
