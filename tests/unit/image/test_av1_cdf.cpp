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
