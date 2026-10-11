// 38_image_sixel.cpp —— 终端内显示图片（按能力选择 kitty / iterm2 / sixel / 半块）
//
// 演示 protocol/ 的完整链路：Bitmap → 探测能力 → 选协议 → 编码 → 写 stdout。
#include <cstdint>

#include "termchart/api/print.hpp"
#include "termchart/image/bitmap.hpp"
#include "termchart/protocol/detect.hpp"
#include "termchart/protocol/fallback.hpp"
#include "termchart/protocol/halfblock_image.hpp"
#include "termchart/protocol/iterm2_inline.hpp"
#include "termchart/protocol/kitty_graphics.hpp"
#include "termchart/protocol/sixel.hpp"

namespace {

using namespace termchart;

// 32×24 的彩色渐变测试图。
image::Bitmap gradient() {
    image::Bitmap bmp(32, 24, 4);
    for (std::uint32_t y = 0; y < bmp.height(); ++y) {
        for (std::uint32_t x = 0; x < bmp.width(); ++x) {
            bmp.set(x, y, 0, static_cast<std::uint8_t>(x * 8));
            bmp.set(x, y, 1, static_cast<std::uint8_t>(y * 10));
            bmp.set(x, y, 2, 200);
            bmp.set(x, y, 3, 255);
        }
    }
    return bmp;
}

}  // namespace

int main() {
    const image::Bitmap bmp = gradient();
    const protocol::ProtocolCaps caps = protocol::detect_protocol_caps();
    const protocol::ProtocolKind kind = protocol::pick_protocol(caps);

    std::string bytes;
    switch (kind) {
        case protocol::ProtocolKind::Kitty:
            bytes = protocol::KittyProtocol{}.encode(bmp);
            break;
        case protocol::ProtocolKind::Iterm2:
            bytes = protocol::Iterm2Inline{}.encode(bmp);
            break;
        case protocol::ProtocolKind::Sixel:
            bytes = protocol::SixelProtocol{}.encode(bmp);
            break;
        case protocol::ProtocolKind::HalfBlock:
            bytes = protocol::HalfBlockProtocol{}.encode(bmp);
            break;
        case protocol::ProtocolKind::None:
            bytes = "[no image protocol detected]\n";
            break;
    }

    const Status st = api::write_stdout(bytes);
    api::flush_stdout();
    return st == ErrorCode::Ok ? 0 : 1;
}
