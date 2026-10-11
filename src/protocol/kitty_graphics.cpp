// kitty_graphics.cpp —— Kitty 图形协议：原始像素 + base64 + 4096 字节分块
#include "termchart/protocol/kitty_graphics.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "internal.hpp"

namespace termchart::protocol {

std::string KittyProtocol::encode(const image::Bitmap& bitmap,
                                  const ProtocolOptions& opts) const {
    (void)opts;  // kitty 负载即像素，缩放交给终端
    if (bitmap.empty()) {
        return {};
    }
    const std::uint32_t w = bitmap.width();
    const std::uint32_t h = bitmap.height();
    const std::uint8_t ch = bitmap.channels();

    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(w) * h * ch);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            for (std::uint8_t c = 0; c < ch; ++c) {
                raw.push_back(bitmap.at(x, y, c));
            }
        }
    }
    const std::string payload = base64_encode(raw.data(), raw.size());

    // 分块：首块带 a=T,f=..,s=,v=；之后每块只带 m=。
    constexpr std::size_t kChunk = 4096;
    std::string out;
    bool first = true;
    for (std::size_t pos = 0; pos < payload.size(); pos += kChunk) {
        const std::size_t take = std::min(kChunk, payload.size() - pos);
        const bool more = pos + take < payload.size();
        out += kEsc;
        out += "_G";
        if (first) {
            out += "a=T,f=";
            out += (ch == 3 ? "24" : "32");
            out += ",s=";
            out += std::to_string(w);
            out += ",v=";
            out += std::to_string(h);
            first = false;
        }
        out += ",m=";
        out += (more ? "1" : "0");
        out += ';';
        out += payload.substr(pos, take);
        out += kEsc;
        out += '\\';
    }
    return out;
}

}  // namespace termchart::protocol
