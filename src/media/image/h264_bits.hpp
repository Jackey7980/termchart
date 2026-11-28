// h264_bits.hpp —— H.264 专属薄封装（NAL 头 / 起始码）
//
// 通用零件（BitWriter、防竞争、Annex-B 起始码）已下沉到
// src/media/encoder/bitstream/bit_writer.hpp，与 HEVC 共用；这里只保留 H.264 的
// 1 字节 NAL 头语义，命名与原实现保持一致，便于两个 TU 无改动迁移。
#ifndef TERMCHART_SRC_IMAGE_H264_BITS_HPP
#define TERMCHART_SRC_IMAGE_H264_BITS_HPP

#include <cstdint>
#include <string>

#include "../encoder/bitstream/bit_writer.hpp"

namespace termchart::image::h264_detail {

using codec::bitstream::BitWriter;  // MSB-first，字段写法与原 BitWriter 完全一致

inline int clamp8(int v) { return codec::bitstream::clamp_u8(v); }

// 防竞争字节（H.264 7.4.1）。
inline std::string add_ep(const std::string& rbsp) {
    return codec::bitstream::emulation_prevention(rbsp);
}

// H.264 NAL：1 字节头 = forbidden_zero_bit(0) | nal_ref_idc(2) | nal_unit_type(5)。
inline std::string make_nal(int nal_ref_idc, int type, const std::string& rbsp) {
    const std::uint8_t hdr = static_cast<std::uint8_t>((nal_ref_idc << 5) | (type & 0x1f));
    return codec::bitstream::annexb_nal(&hdr, 1, rbsp);
}

}  // namespace termchart::image::h264_detail

#endif  // TERMCHART_SRC_IMAGE_H264_BITS_HPP
