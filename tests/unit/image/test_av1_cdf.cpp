// test_av1_cdf.cpp —— 默认 CDF 表接线与多符号编解码对称
#include <doctest.h>

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
        av1::DaalaEncoder enc;
        av1::write_symbol(enc, cdf, N_INTRA_PRED_MODES, sym);
        const std::string bytes = enc.finish();
        av1::DaalaDecoder dec;
        REQUIRE(dec.init(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()));
        int got = -1;
        REQUIRE(av1::read_symbol(dec, cdf, N_INTRA_PRED_MODES, got));
        CHECK(got == sym);
    }
}
