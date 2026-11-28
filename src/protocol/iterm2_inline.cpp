// iterm2_inline.cpp —— iTerm2 内联图像（PNG + base64 + OSC 1337）
#include "termchart/protocol/iterm2_inline.hpp"

#include <cstdint>
#include <string>

#include "internal.hpp"
#include "termchart/media/image/png_encode.hpp"

namespace termchart::protocol {

std::string Iterm2Inline::encode(const image::Bitmap& bitmap,
                                 const ProtocolOptions& opts) const {
    (void)opts;  // iTerm2 的 width/height 直接用像素尺寸，不需要按格换算
    if (bitmap.empty()) {
        return {};
    }
    const std::string png = image::encode_png(bitmap);
    if (png.empty()) {
        return {};
    }

    std::string out;
    out.reserve(png.size() * 4 / 3 + 64);
    out += "\x1b]1337;File=inline=1;size=";
    out += std::to_string(png.size());
    out += ";width=";
    out += std::to_string(bitmap.width());
    out += "px;height=";
    out += std::to_string(bitmap.height());
    out += "px:";
    out += base64_encode(reinterpret_cast<const std::uint8_t*>(png.data()), png.size());
    out.push_back('\a');  // BEL 结束 OSC
    return out;
}

}  // namespace termchart::protocol
