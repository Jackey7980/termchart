#include "cdf_manager.hpp"

namespace termchart::image::codec::av1 {

void write_symbol(RangeEncoder& enc, const std::uint16_t* cdf, int n, int symbol) {
    enc.write_cdf(cdf, n, symbol);
}

unsigned read_symbol(RangeDecoder& dec, const std::uint16_t* cdf, int n) {
    if (n <= 1) return 0;
    return dec.decode_symbol(cdf, n);
}

void update_cdf(std::uint16_t* cdf, int n, unsigned symbol) {
    if (symbol >= static_cast<unsigned>(n)) return;
    const unsigned count = cdf[n];
    const unsigned rate = 4 + (count >> 4) + (n > 3 ? 1u : 0u);
    unsigned i = 0;
    for (; i < symbol; ++i) cdf[i] += static_cast<std::uint16_t>((32768 - cdf[i]) >> rate);
    for (; i < static_cast<unsigned>(n); ++i) cdf[i] -= static_cast<std::uint16_t>(cdf[i] >> rate);
    if (count < 32) cdf[n] = static_cast<std::uint16_t>(count + 1);
}

void write_symbol_adaptive(RangeEncoder& enc, std::uint16_t* cdf, int n, int symbol) {
    write_symbol(enc, cdf, n, symbol);
    update_cdf(cdf, n, static_cast<unsigned>(symbol));
}

unsigned read_symbol_adaptive(RangeDecoder& dec, std::uint16_t* cdf, int n) {
    const unsigned symbol = read_symbol(dec, cdf, n);
    update_cdf(cdf, n, symbol);
    return symbol;
}

}  // namespace termchart::image::codec::av1
