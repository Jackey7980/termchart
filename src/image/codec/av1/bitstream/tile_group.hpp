// tile_group.hpp —— 单 tile 的 8x8 帧内块语法（DC_PRED + 全零残差）
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_TILE_GROUP_HPP
#define TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_TILE_GROUP_HPP

#include <string>

namespace termchart::image::codec::av1 {

// 返回一个 OBU_TILE_GROUP 的完整 payload（含 tile 头与 tile_size 字节）。
std::string write_tile_group_8x8();

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_TILE_GROUP_HPP
