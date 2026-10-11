// test_protocol_sixel.cpp —— Sixel 编码器结构
#include <doctest.h>

#include <string>

#include "termchart/image/bitmap.hpp"
#include "termchart/protocol/sixel.hpp"

namespace {

using namespace termchart;
using namespace termchart::protocol;

}  // namespace

TEST_CASE("protocol/sixel: 空位图返回空串") {
    SixelProtocol p;
    CHECK(p.encode(image::Bitmap{}).empty());
    const image::Bitmap zero(0, 0, 4);
    CHECK(p.encode(zero).empty());
}

TEST_CASE("protocol/sixel: DCS 引入 / 光栅属性 / ST 收口") {
    image::Bitmap bmp(2, 2, 4);
    bmp.fill(255, 0, 0, 255);
    SixelProtocol p;
    const std::string s = p.encode(bmp);

    CHECK(s.rfind("\x1bP0;0;0q", 0) == 0);
    CHECK(s.find("\"1;1;2;2") != std::string::npos);
    CHECK(s.find('#') != std::string::npos);  // 调色板定义
    REQUIRE(s.size() >= 2);
    CHECK(s.compare(s.size() - 2, 2, "\x1b\\") == 0);
}

TEST_CASE("protocol/sixel: 单色大块触发游程压缩") {
    image::Bitmap bmp(8, 6, 4);
    bmp.fill(0, 0, 255, 255);
    SixelProtocol p;

    ProtocolOptions rle;
    rle.rle = true;
    const std::string compressed = p.encode(bmp, rle);
    CHECK(compressed.find('!') != std::string::npos);

    ProtocolOptions plain;
    plain.rle = false;
    const std::string raw = p.encode(bmp, plain);
    CHECK(raw.find('!') == std::string::npos);
}

TEST_CASE("protocol/sixel: 多带图像用 - 分隔") {
    image::Bitmap bmp(2, 13, 4);  // 13 行 → 3 带
    bmp.fill(0, 128, 0, 255);
    SixelProtocol p;
    ProtocolOptions plain;
    plain.rle = false;
    const std::string s = p.encode(bmp, plain);
    CHECK(s.find('-') != std::string::npos);
}
