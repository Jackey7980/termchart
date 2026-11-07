// ep_nal.hpp —— 防竞争字节与 Annex-B 起始码封装（L5 通用比特流基建）
//
// 无 H.264 / HEVC 语义，两个编码器与容器层共用。实现在 ep_nal.cpp。
#ifndef TERMCHART_SRC_IMAGE_CODEC_BITSTREAM_EP_NAL_HPP
#define TERMCHART_SRC_IMAGE_CODEC_BITSTREAM_EP_NAL_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace termchart::image::codec::bitstream {

// 00 00 后紧跟 <=0x03 的字节时插入 0x03（H.264 7.4.1 / H.265 7.4.2 同构）。
std::string add_ep(const std::string& rbsp);

// header 是 NAL 头原始字节（H.264 1 字节 / H.265 2 字节），先写
// 00 00 00 01 起始码与头，再追加防竞争后的载荷。
std::string annexb_nal(const std::uint8_t* header, std::size_t header_size,
                       const std::string& rbsp);

// 兼容既有调用点（h264_bits.hpp）；语义等同 add_ep。
inline std::string emulation_prevention(const std::string& rbsp) { return add_ep(rbsp); }

}  // namespace termchart::image::codec::bitstream

#endif  // TERMCHART_SRC_IMAGE_CODEC_BITSTREAM_EP_NAL_HPP
