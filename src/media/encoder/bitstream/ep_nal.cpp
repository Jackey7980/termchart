// ep_nal.cpp —— 防竞争字节与 Annex-B 起始码封装实现
#include "ep_nal.hpp"

namespace termchart::image::codec::bitstream {

std::string add_ep(const std::string& rbsp) {
    std::string out;
    out.reserve(rbsp.size() + rbsp.size() / 64 + 4);
    int zeros = 0;
    for (char ch : rbsp) {
        const int c = static_cast<unsigned char>(ch);
        if (zeros >= 2 && c <= 3) {
            out.push_back(static_cast<char>(3));
            zeros = 0;
        }
        out.push_back(ch);
        zeros = (c == 0) ? zeros + 1 : 0;
    }
    return out;
}

std::string annexb_nal(const std::uint8_t* header, std::size_t header_size,
                       const std::string& rbsp) {
    std::string out;
    out.reserve(header_size + rbsp.size() + 8);
    out.push_back(static_cast<char>(0));
    out.push_back(static_cast<char>(0));
    out.push_back(static_cast<char>(0));
    out.push_back(static_cast<char>(1));  // start code 00 00 00 01
    for (std::size_t i = 0; i < header_size; ++i)
        out.push_back(static_cast<char>(header[i]));
    out += add_ep(rbsp);
    return out;
}

}  // namespace termchart::image::codec::bitstream
