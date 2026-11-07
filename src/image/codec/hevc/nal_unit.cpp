// nal_unit.cpp —— HEVC NAL 单元头实现
#include "nal_unit.hpp"

#include <cstdint>

#include "../bitstream/ep_nal.hpp"

namespace termchart::image::codec::hevc {

std::string make_nal(int type, const std::string& rbsp) {
    const unsigned header = (static_cast<unsigned>(type) << 9) | 1u;  // layer=0, temporal=1
    const std::uint8_t hdr[2] = {
        static_cast<std::uint8_t>((header >> 8) & 0xFF),
        static_cast<std::uint8_t>(header & 0xFF),
    };
    return bitstream::annexb_nal(hdr, 2, rbsp);
}

}  // namespace termchart::image::codec::hevc
