// test_protocol_fallback.cpp —— 协议降级与命名
#include <doctest.h>

#include <string>

#include "termchart/protocol/fallback.hpp"
#include "termchart/protocol/halfblock_image.hpp"
#include "termchart/image/bitmap.hpp"

namespace {

using namespace termchart;
using namespace termchart::protocol;

}  // namespace

TEST_CASE("protocol/fallback: 优先级 Kitty > Iterm2 > Sixel > HalfBlock > None") {
    ProtocolCaps caps;
    CHECK(pick_protocol(caps) == ProtocolKind::None);

    caps.truecolor = true;
    CHECK(pick_protocol(caps) == ProtocolKind::HalfBlock);

    caps.sixel = true;
    CHECK(pick_protocol(caps) == ProtocolKind::Sixel);

    caps.iterm2 = true;
    CHECK(pick_protocol(caps) == ProtocolKind::Iterm2);

    caps.kitty = true;
    CHECK(pick_protocol(caps) == ProtocolKind::Kitty);
}

TEST_CASE("protocol/fallback: 只有 iterm2 时选 Iterm2（PNG 编码器已就位）") {
    ProtocolCaps caps;
    caps.iterm2 = true;
    CHECK(pick_protocol(caps) == ProtocolKind::Iterm2);
}

TEST_CASE("protocol: 协议命名") {
    CHECK(std::string(protocol_name(ProtocolKind::None)) == "none");
    CHECK(std::string(protocol_name(ProtocolKind::Sixel)) == "sixel");
    CHECK(std::string(protocol_name(ProtocolKind::Kitty)) == "kitty");
    CHECK(std::string(protocol_name(ProtocolKind::Iterm2)) == "iterm2");
    CHECK(std::string(protocol_name(ProtocolKind::HalfBlock)) == "halfblock");
}

TEST_CASE("protocol/halfblock: 每格输出 ▀ 与真彩前景/背景") {
    image::Bitmap bmp(1, 2, 3);
    bmp.set(0, 0, 0, 255);  // 上：红
    bmp.set(0, 0, 1, 0);
    bmp.set(0, 0, 2, 0);
    bmp.set(0, 1, 0, 0);    // 下：蓝
    bmp.set(0, 1, 1, 0);
    bmp.set(0, 1, 2, 255);

    HalfBlockProtocol p;
    const std::string s = p.encode(bmp);
    CHECK(s.find("\x1b[38;2;255;0;0m") != std::string::npos);  // 前景 = 上像素
    CHECK(s.find("\x1b[48;2;0;0;255m") != std::string::npos);  // 背景 = 下像素
    CHECK(s.find("\u2580") != std::string::npos);
    CHECK(s.find("\x1b[0m") != std::string::npos);
}
