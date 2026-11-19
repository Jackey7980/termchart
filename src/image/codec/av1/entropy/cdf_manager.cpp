#include "cdf_manager.hpp"

namespace termchart::image::codec::av1 {

void write_symbol(RangeEncoder& enc, const std::uint16_t* cdf, int n, int symbol) {
    if (n <= 1) return;
    if (symbol < 0) symbol = 0;
    if (symbol >= n) symbol = n - 1;

    const unsigned r_hi8 = enc.range() >> 8;
    const unsigned upper =
        symbol > 0 ? range_split(r_hi8, cdf[symbol - 1], static_cast<unsigned>(n - symbol + 1))
                   : enc.range();
    const unsigned lower =
        symbol + 1 < n ? range_split(r_hi8, cdf[symbol], static_cast<unsigned>(n - symbol)) : 0u;
    enc.write_interval(lower, upper - lower);
}

unsigned read_symbol(RangeDecoder& dec, const std::uint16_t* cdf, int n) {
    if (n <= 1) return 0;
    return dec.decode_symbol(cdf, n);
}

void update_cdf(std::uint16_t* cdf, int n, unsigned symbol) {
    if (symbol >= static_cast<unsigned>(n)) return;
    const unsigned count = cdf[n];
    const unsigned rate = 4 + (count >> 4) + (n > 2 ? 1u : 0u);
    unsigned i = 0;
    for (; i < symbol; ++i) cdf[i] += static_cast<std::uint16_t>((32768 - cdf[i]) >> rate);
    for (; i < static_cast<unsigned>(n); ++i) cdf[i] -= static_cast<std::uint16_t>(cdf[i] >> rate);
    if (count < 32) cdf[n] = static_cast<std::uint16_t>(count + 1);
}

}  // namespace termchart::image::codec::av1
