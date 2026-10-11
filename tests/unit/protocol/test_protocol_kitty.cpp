// test_protocol_kitty.cpp —— Kitty 图形协议编码
#include <doctest.h>

#include <cstddef>
#include <string>

#include "termchart/image/bitmap.hpp"
#include "termchart/protocol/kitty_graphics.hpp"

namespace {

using namespace termchart;
using namespace termchart::protocol;

std::size_t count_esc_g(const std::string& s) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = s.find("\x1b_G", pos)) != std::string::npos) {
        ++count;
        pos += 3;
    }
    return count;
}

}  // namespace

TEST_CASE("protocol/kitty: 头部参数与 ST 收口") {
    image::Bitmap bmp(2, 2, 4);
    bmp.fill(10, 20, 30, 255);
    KittyProtocol p;
    const std::string s = p.encode(bmp);

    CHECK(s.rfind("\x1b_G", 0) == 0);
    CHECK(s.find("a=T,f=32,s=2,v=2") != std::string::npos);
    CHECK(s.find(",m=0;") != std::string::npos);
    REQUIRE(s.size() >= 2);
    CHECK(s.compare(s.size() - 2, 2, "\x1b\\") == 0);
}

TEST_CASE("protocol/kitty: 3 通道用 f=24") {
    image::Bitmap bmp(1, 1, 3);
    bmp.fill(1, 2, 3);
    KittyProtocol p;
    CHECK(p.encode(bmp).find("f=24") != std::string::npos);
}

TEST_CASE("protocol/kitty: base64 载荷正确（全零 → AAAAAA==）") {
    const image::Bitmap bmp(1, 1, 4);  // 默认全 0
    KittyProtocol p;
    CHECK(p.encode(bmp).find("AAAAAA==") != std::string::npos);
}

TEST_CASE("protocol/kitty: 大负载分块 m=1…m=0") {
    image::Bitmap bmp(64, 64, 4);  // 16384 字节 → base64 21848 → 6 块
    bmp.fill(1, 2, 3, 4);
    KittyProtocol p;
    const std::string s = p.encode(bmp);
    CHECK(s.find(",m=1;") != std::string::npos);
    CHECK(s.find(",m=0;") != std::string::npos);
    CHECK(count_esc_g(s) == 6);
}

TEST_CASE("protocol/kitty: 空位图返回空串") {
    KittyProtocol p;
    CHECK(p.encode(image::Bitmap{}).empty());
}
