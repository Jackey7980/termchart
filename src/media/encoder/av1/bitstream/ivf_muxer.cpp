#include "ivf_muxer.hpp"

namespace termchart::image::codec::av1 {
namespace {

void put_le16(std::string& out, std::uint16_t v) {
    out.push_back(static_cast<char>(v & 0xff));
    out.push_back(static_cast<char>((v >> 8) & 0xff));
}

void put_le32(std::string& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
}

void put_le64(std::string& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
}

}  // namespace

std::string write_ivf_header(int width, int height, int frame_rate, std::uint32_t frame_count) {
    std::string out;
    out.reserve(32);
    out.append("DKIF", 4);
    put_le16(out, 0);   // version
    put_le16(out, 32);  // header length
    out.append("AV01", 4);
    put_le16(out, static_cast<std::uint16_t>(width));
    put_le16(out, static_cast<std::uint16_t>(height));
    put_le32(out, static_cast<std::uint32_t>(frame_rate > 0 ? frame_rate : 30));
    put_le32(out, 1);  // timebase scale
    put_le32(out, frame_count);
    put_le32(out, 0);  // unused
    return out;
}

std::string write_ivf_frame(std::string_view obu_stream, std::uint64_t timestamp) {
    std::string out;
    out.reserve(12 + obu_stream.size());
    put_le32(out, static_cast<std::uint32_t>(obu_stream.size()));
    put_le64(out, timestamp);
    out.append(obu_stream);
    return out;
}

}  // namespace termchart::image::codec::av1
