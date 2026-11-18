// obu_header.hpp —— AV1 OBU 头、LEB128 尺寸与最小 OBU 封装（规范 5.3.2 / 5.3.3 / 6.2.2）
//
// 本阶段只写「无 extension、带 size 字段」这一种头形：
//   forbidden_bit(1)=0 | obu_type(4) | extension_flag(1)=0 | has_size(1)=1 | reserved(1)=0
// 真实值据此固定：SEQUENCE_HEADER=0x0A、TEMPORAL_DELIMITER=0x12、
// FRAME_HEADER=0x1A、TILE_GROUP=0x22、FRAME=0x2A。
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_OBU_HEADER_HPP
#define TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_OBU_HEADER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace termchart::image::codec::av1 {

enum class ObuType : std::uint8_t {
    SequenceHeader = 1,
    TemporalDelimiter = 2,
    FrameHeader = 3,
    TileGroup = 4,
    Metadata = 5,
    Frame = 6,
    RedundantFrameHeader = 7,
    TileList = 8,
    Padding = 15,
};

// 无符号 LEB128：低 7 位先发，第 8 位为 continuation。
std::string write_leb128(std::uint64_t value);

// 单个 OBU：1 字节头（has_size=1）+ LEB128(payload.size()) + payload。
std::string make_obu(ObuType type, std::string_view payload);

// OBU_TEMPORAL_DELIMITER：头 0x12 + size=0（0x00），无 payload。
std::string make_temporal_delimiter();

// 回读单个 OBU（用于单测与后续解码器对接）；失败返回 false。
bool read_obu(std::string_view bytes, std::size_t pos, ObuType& type,
              std::string_view& payload, std::size_t& consumed);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_OBU_HEADER_HPP
