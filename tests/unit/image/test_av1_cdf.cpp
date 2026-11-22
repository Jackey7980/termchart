// test_av1_cdf.cpp —— 默认 CDF 表接线与多符号编解码对称
#include <doctest.h>

#include <array>
#include <cstdint>
#include <string>

#include "src/image/codec/av1/entropy/cdf_manager.hpp"
#include "src/image/codec/av1/entropy/default_cdf.hpp"

namespace av1 = termchart::image::codec::av1;

TEST_CASE("av1: 默认 CDF 表可读且关键上下文非零") {
    const auto& cdf = av1::kDefaultCdf;
    CHECK(cdf.m.partition[BL_8X8][0][0] > 0);
    CHECK(cdf.kfym[0][0][0] > 0);
    CHECK(cdf.m.uv_mode[0][DC_PRED][0] > 0);
}

TEST_CASE("av1: 多符号 CDF 编解码回环") {
    const std::uint16_t* cdf = av1::kDefaultCdf.kfym[0][0];
    for (int sym = 0; sym < N_INTRA_PRED_MODES; ++sym) {
        av1::RangeEncoder enc;
        av1::write_symbol(enc, cdf, N_INTRA_PRED_MODES, sym);
        const std::string bytes = enc.finish();
        av1::RangeDecoder dec;
        dec.init(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
        CHECK(av1::read_symbol(dec, cdf, N_INTRA_PRED_MODES) == static_cast<unsigned>(sym));
    }
}

TEST_CASE("av1: 自适应更新后多符号编解码仍一致") {
    std::array<std::uint16_t, N_INTRA_PRED_MODES + 1> enc_cdf{};
    std::array<std::uint16_t, N_INTRA_PRED_MODES + 1> dec_cdf{};
    for (int i = 0; i < N_INTRA_PRED_MODES; ++i) {
        enc_cdf[static_cast<std::size_t>(i)] = av1::kDefaultCdf.kfym[0][0][i];
        dec_cdf[static_cast<std::size_t>(i)] = av1::kDefaultCdf.kfym[0][0][i];
    }

    std::uint32_t seed = 0xC0FFEEu;
    std::array<int, 256> symbols{};
    for (auto& s : symbols) {
        seed = seed * 1664525u + 1013904223u;
        s = static_cast<int>(seed % N_INTRA_PRED_MODES);
    }

    av1::RangeEncoder enc;
    for (int sym : symbols) {
        av1::write_symbol(enc, enc_cdf.data(), N_INTRA_PRED_MODES, sym);
        av1::update_cdf(enc_cdf.data(), N_INTRA_PRED_MODES, static_cast<unsigned>(sym));
    }
    const std::string bytes = enc.finish();

    av1::RangeDecoder dec;
    dec.init(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    for (int sym : symbols) {
        const unsigned got = av1::read_symbol(dec, dec_cdf.data(), N_INTRA_PRED_MODES);
        REQUIRE(got == static_cast<unsigned>(sym));
        av1::update_cdf(dec_cdf.data(), N_INTRA_PRED_MODES, got);
    }
}

namespace {
std::array<std::uint16_t, N_INTRA_PRED_MODES + 1> reference_update(
    std::array<std::uint16_t, N_INTRA_PRED_MODES + 1> cdf, int n, int sym) {
    const int count = cdf[static_cast<std::size_t>(n)];
    const int rate = 4 + (count >> 4) + (n > 3 ? 1 : 0);
    for (int i = 0; i < n - 1; ++i) {
        const int up = cdf[static_cast<std::size_t>(i)] +
                       ((32768 - cdf[static_cast<std::size_t>(i)]) >> rate);
        const int down = cdf[static_cast<std::size_t>(i)] -
                         (cdf[static_cast<std::size_t>(i)] >> rate);
        cdf[static_cast<std::size_t>(i)] =
            static_cast<std::uint16_t>(i < sym ? up : down);
    }
    if (count < 32)
        cdf[static_cast<std::size_t>(n)] = static_cast<std::uint16_t>(count + 1);
    return cdf;
}
}  // namespace

TEST_CASE("av1: CDF 自适应更新与 libaom 公式一致") {
    constexpr int n = N_INTRA_PRED_MODES;
    for (int sym = 0; sym < n; ++sym) {
        std::array<std::uint16_t, n + 1> cdf{};
        for (int i = 0; i < n; ++i)
            cdf[static_cast<std::size_t>(i)] = av1::kDefaultCdf.kfym[0][0][i];
        const auto expected = reference_update(cdf, n, sym);
        av1::update_cdf(cdf.data(), n, static_cast<unsigned>(sym));
        CHECK(cdf == expected);
        for (int i = 1; i < n; ++i)
            CHECK(cdf[static_cast<std::size_t>(i - 1)] >= cdf[static_cast<std::size_t>(i)]);
    }
}

TEST_CASE("av1: 自适应状态机封装往返且计数饱和") {
    constexpr int n = N_INTRA_PRED_MODES;
    std::array<std::uint16_t, n + 1> enc_cdf{}, dec_cdf{};
    for (int i = 0; i < n; ++i) {
        enc_cdf[static_cast<std::size_t>(i)] = av1::kDefaultCdf.kfym[0][0][i];
        dec_cdf[static_cast<std::size_t>(i)] = av1::kDefaultCdf.kfym[0][0][i];
    }
    std::array<int, 512> symbols{};
    std::uint32_t seed = 0x5EEDu;
    for (auto& s : symbols) {
        seed = seed * 1664525u + 1013904223u;
        s = static_cast<int>(seed % n);
    }

    av1::RangeEncoder enc;
    for (int sym : symbols)
        av1::write_symbol_adaptive(enc, enc_cdf.data(), n, sym);
    const std::string bytes = enc.finish();

    av1::RangeDecoder dec;
    dec.init(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    for (int sym : symbols)
        REQUIRE(av1::read_symbol_adaptive(dec, dec_cdf.data(), n) ==
                static_cast<unsigned>(sym));

    CHECK(enc_cdf == dec_cdf);
    CHECK(enc_cdf[static_cast<std::size_t>(n)] == 32);
}

TEST_CASE("av1: 极端偏斜 CDF 的区间切分不吞并符号") {
    // mv_joint 这类表：符号 0 约 95%，其余三个符号合计 5%。
    // 每个符号单独成流时必须能在解码端读出原符号。
    constexpr std::uint16_t skewed[4] = {1639, 656, 164, 0};
    for (int symbol = 0; symbol < 4; ++symbol) {
        av1::RangeEncoder enc;
        enc.write_cdf(skewed, 4, symbol);
        const std::string bytes = enc.finish();
        av1::RangeDecoder dec;
        dec.init(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
        CHECK(dec.decode_symbol(skewed, 4) == static_cast<unsigned>(symbol));
    }

    // 512 符号长流混合 0 与稀有符号，检验重归一化补位不会把稀有符号吞成 0。
    std::array<int, 512> symbols{};
    for (int i = 0; i < 512; ++i)
        symbols[static_cast<std::size_t>(i)] = i % 8 == 0 ? 1 + i % 3 : 0;
    av1::RangeEncoder enc;
    for (int s : symbols) enc.write_cdf(skewed, 4, s);
    const std::string bytes = enc.finish();
    av1::RangeDecoder dec;
    dec.init(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    for (int i = 0; i < 512; ++i)
        REQUIRE(dec.decode_symbol(skewed, 4) ==
                static_cast<unsigned>(symbols[static_cast<std::size_t>(i)]));
}
