// ivf_muxer.hpp —— IVF（IVF 1.0）容器：32 字节文件头 + 每帧 12 字节头
//
// 小端字段，fourcc = "AV01"。测试期用 ffmpeg/libdav1d 直接解该容器。
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_IVF_MUXER_HPP
#define TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_IVF_MUXER_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace termchart::image::codec::av1 {

std::string write_ivf_header(int width, int height, int frame_rate = 30,
                             std::uint32_t frame_count = 1);
std::string write_ivf_frame(std::string_view obu_stream, std::uint64_t timestamp = 0);

}  // namespace termchart::image::codec::av1

#endif  // TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_IVF_MUXER_HPP
