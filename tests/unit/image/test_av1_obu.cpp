// test_av1_obu.cpp —— OBU 头 / LEB128 / IVF 容器的字节级回环
#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "src/image/codec/av1/bitstream/ivf_muxer.hpp"
#include "src/image/codec/av1/bitstream/obu_header.hpp"
#include "src/image/codec/av1/bitstream/tile_group.hpp"
#include "src/image/codec/av1/entropy/cdf_manager.hpp"
#include "src/image/codec/av1/entropy/default_cdf.hpp"
#include "src/image/codec/av1/entropy/range_coder.hpp"

namespace av1 = termchart::image::codec::av1;

namespace {

// 8x8 单块的块内语法符号：skip / Y 模式 / use_filter_intra / UV 模式。
struct TileSymbols {
    int skip;
    unsigned y;
    int filter_intra;
    unsigned uv;
};

TileSymbols decode_tile(const std::uint8_t* data, std::size_t size) {
    const auto& m = av1::kDefaultCdf;
    av1::RangeDecoder dec;
    dec.init(data, size);
    TileSymbols s{};
    s.skip = dec.decode_bool(m.m.skip[0][0]);
    s.y = av1::read_symbol(dec, m.kfym[0][0], N_INTRA_PRED_MODES);
    s.filter_intra = dec.decode_bool(m.m.use_filter_intra[0][0]);
    s.uv = av1::read_symbol(dec, m.m.uv_mode[1][s.y], N_UV_INTRA_PRED_MODES);
    return s;
}

}  // namespace

TEST_CASE("av1: LEB128 编码与边界") {
    CHECK(av1::write_leb128(0) == std::string("\0", 1));
    CHECK(av1::write_leb128(1) == std::string("\1", 1));
    CHECK(av1::write_leb128(127) == std::string("\x7f", 1));
    CHECK(av1::write_leb128(128) == std::string("\x80\x01", 2));
    CHECK(av1::write_leb128(16383) == std::string("\xff\x7f", 2));
    CHECK(av1::write_leb128(16384) == std::string("\x80\x80\x01", 3));
}

TEST_CASE("av1: OBU 头字节与回读") {
    const av1::ObuType types[] = {
        av1::ObuType::SequenceHeader,   av1::ObuType::TemporalDelimiter,
        av1::ObuType::FrameHeader,      av1::ObuType::TileGroup,
    };
    const std::uint8_t headers[] = {0x0a, 0x12, 0x1a, 0x22};
    for (std::size_t i = 0; i < 4; ++i) {
        const std::string payload = std::string("abcd\0xyz", 8);
        const std::string obu = av1::make_obu(types[i], payload);
        CHECK(static_cast<unsigned char>(obu[0]) == headers[i]);
        av1::ObuType got = av1::ObuType::Padding;
        std::string_view body;
        std::size_t used = 0;
        REQUIRE(av1::read_obu(obu, 0, got, body, used));
        CHECK(got == types[i]);
        CHECK(body == std::string_view(payload));
        CHECK(used == obu.size());
    }
    CHECK(av1::make_temporal_delimiter() == std::string("\x12\0", 2));
}

TEST_CASE("av1: IVF 头字段与帧长") {
    const std::string h = av1::write_ivf_header(8, 8, 30, 1);
    REQUIRE(h.size() == 32);
    CHECK(h.substr(0, 4) == "DKIF");
    CHECK(h.substr(8, 4) == "AV01");
    CHECK(static_cast<unsigned char>(h[4]) == 0);   // version
    CHECK(static_cast<unsigned char>(h[6]) == 32);  // header length
    CHECK(static_cast<unsigned char>(h[12]) == 8);  // width lo
    CHECK(static_cast<unsigned char>(h[14]) == 8);  // height lo

    const std::string frame = av1::write_ivf_frame(std::string(7, 'z'), 3);
    REQUIRE(frame.size() == 19);
    CHECK(static_cast<unsigned char>(frame[0]) == 7);
    CHECK(static_cast<unsigned char>(frame[4]) == 3);
    CHECK(frame.substr(12) == std::string(7, 'z'));
}

TEST_CASE("av1: 最小 tile group 字节序列") {
    // 单 tile 头 0x40（flag + trailing bits）+ tile_size=1 + 区间编码负载。
    const std::string tg = av1::write_tile_group_8x8();
    REQUIRE(tg.size() == 3);
    CHECK(static_cast<unsigned char>(tg[0]) == 0x40);
    CHECK(static_cast<unsigned char>(tg[1]) == 0x01);

    const TileSymbols sym =
        decode_tile(reinterpret_cast<const std::uint8_t*>(tg.data()) + 2, 1);
    CHECK(sym.skip == 0);
    CHECK(sym.y == static_cast<unsigned>(SMOOTH_PRED));
    CHECK(sym.filter_intra == 1);
    CHECK(sym.uv == static_cast<unsigned>(DC_PRED));
}

TEST_CASE("av1: 自研 tile 负载与 libaom 参考单字节同解") {
    // libaom 参考流该帧的 tile 负载是单字节 0xC0；自研编码器收尾补位不同，
    // 落在同一码字区间的另一合法值。两者解出的符号序列必须完全一致，
    // 与 ffmpeg/libdav1d 真解码一致性（tools/verify_av1_minimal.py）互补。
    const std::uint8_t ref = 0xC0;
    const TileSymbols sym = decode_tile(&ref, 1);
    CHECK(sym.skip == 0);
    CHECK(sym.y == static_cast<unsigned>(SMOOTH_PRED));
    CHECK(sym.filter_intra == 1);
    CHECK(sym.uv == static_cast<unsigned>(DC_PRED));
}
