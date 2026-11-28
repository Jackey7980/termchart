// test_jpeg_encode.cpp —— 自包含 baseline JPEG 编码器最小测试
#include "termchart/media/image/jpeg_encode.hpp"

#include <cstdint>
#include <string>

#include <doctest.h>

using namespace termchart::image;

namespace {

bool has_marker(const std::string& b, std::uint8_t marker) {
    for (std::size_t i = 0; i + 1 < b.size(); ++i) {
        if (static_cast<std::uint8_t>(b[i]) == 0xFF &&
            static_cast<std::uint8_t>(b[i + 1]) == marker) {
            return true;
        }
    }
    return false;
}

Bitmap gradient(int w, int h) {
    Bitmap bm(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), 3);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            bm.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 0,
                   static_cast<std::uint8_t>(x * 255 / w));
            bm.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 1,
                   static_cast<std::uint8_t>(y * 255 / h));
            bm.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 2,
                   static_cast<std::uint8_t>((x + y) * 128 / (w + h)));
        }
    return bm;
}

}  // namespace

TEST_CASE("jpeg: 空位图返回空串") {
    Bitmap empty;
    CHECK(encode_jpeg(empty).empty());
    Bitmap zero(0, 10);
    CHECK(encode_jpeg(zero).empty());
}

TEST_CASE("jpeg: 产出合规 JFIF 结构") {
    auto jpg = encode_jpeg(gradient(32, 24), 85);
    REQUIRE_FALSE(jpg.empty());
    // SOI / EOI
    CHECK(jpg.size() >= 2);
    CHECK(static_cast<std::uint8_t>(jpg[0]) == 0xFF);
    CHECK(static_cast<std::uint8_t>(jpg[1]) == 0xD8);
    CHECK(static_cast<std::uint8_t>(jpg[jpg.size() - 2]) == 0xFF);
    CHECK(static_cast<std::uint8_t>(jpg[jpg.size() - 1]) == 0xD9);
    // 关键标记齐全
    CHECK(has_marker(jpg, 0xE0));  // APP0（JFIF）
    CHECK(has_marker(jpg, 0xDB));  // DQT
    CHECK(has_marker(jpg, 0xC0));  // SOF0（baseline）
    CHECK(has_marker(jpg, 0xC4));  // DHT
    CHECK(has_marker(jpg, 0xDA));  // SOS
    // APP0 紧跟 "JFIF"
    bool jfif = false;
    for (std::size_t i = 0; i + 6 < jpg.size(); ++i) {
        if (static_cast<std::uint8_t>(jpg[i]) == 0xFF &&
            static_cast<std::uint8_t>(jpg[i + 1]) == 0xE0 &&
            jpg.compare(i + 4, 4, "JFIF") == 0) {
            jfif = true;
            break;
        }
    }
    CHECK(jfif);
}

TEST_CASE("jpeg: 不同质量都产出非空且自洽") {
    auto bm = gradient(40, 30);
    for (int q : {1, 50, 85, 100}) {
        auto j = encode_jpeg(bm, q);
        CHECK_FALSE(j.empty());
        CHECK(static_cast<std::uint8_t>(j[0]) == 0xFF);
        CHECK(static_cast<std::uint8_t>(j[1]) == 0xD8);
        CHECK(has_marker(j, 0xDA));
    }
}
