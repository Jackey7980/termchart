#include "cdf_manager.hpp"

#include <algorithm>

namespace termchart::image::codec::av1 {

void write_symbol(DaalaEncoder& enc, const std::uint16_t* cdf, int n, int symbol) {
    if (symbol < 0) symbol = 0;
    if (symbol >= n) symbol = n - 1;
    // dav1d 的 CDF[0] 是保留槽；阈值从 CDF[1] 开始，min_prob = 4*(n-val)。
    for (int i = 0; i < n - 1; ++i) {
        const int val = i + 1;
        const int min_prob = 4 * (n - val);
        enc.write_bit(symbol > i ? 1 : 0, static_cast<int>(cdf[val]), min_prob);
    }
}

bool read_symbol(DaalaDecoder& dec, const std::uint16_t* cdf, int n, int& symbol) {
    if (n <= 0) return false;
    symbol = n - 1;
    for (int i = 0; i < n - 1; ++i) {
        const int val = i + 1;
        const int min_prob = 4 * (n - val);
        int bit = 0;
        if (!dec.read_bit(static_cast<int>(cdf[val]), bit, min_prob)) return false;
        if (bit == 0) {
            symbol = i;
            return true;
        }
    }
    return true;
}

}  // namespace termchart::image::codec::av1
