// test_av1_obu.cpp —— OBU 头 / LEB128 / IVF 容器的字节级回环
#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "src/image/codec/av1/bitstream/ivf_muxer.hpp"
#include "src/image/codec/av1/bitstream/obu_header.hpp"
#include "src/image/codec/av1/bitstream/tile_group.hpp"

namespace av1 = termchart::image::codec::av1;

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
    // 单 tile 头 0x40（flag + trailing bits）+ tile_size=1 + tile_data=0xC0。
    CHECK(av1::write_tile_group_8x8() == std::string("\x40\x01\xc0", 3));
}
