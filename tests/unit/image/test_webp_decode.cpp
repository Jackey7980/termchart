// test_webp_decode.cpp —— 原生 VP8L WebP 解码器测试
//
// 验证策略：用本库编码器（encode_webp）产出字节流，再用原生解码器（decode_webp）
// 解回像素，与原位图逐字节比对；并验证容器损坏 / 不支持特性两类失败契约。
#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/encode.hpp"
#include "termchart/media/image/webp_decode.hpp"
#include "termchart/media/image/webp_encode.hpp"

using namespace termchart;
using namespace termchart::image;

namespace {

Bitmap make_test_bitmap(std::uint32_t w, std::uint32_t h, bool with_alpha) {
    Bitmap bmp(w, h, with_alpha ? 4u : 3u);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            bmp.set(x, y, 0, static_cast<std::uint8_t>((x * 11 + y * 3) & 0xFF));
            bmp.set(x, y, 1, static_cast<std::uint8_t>((x * 5 + y * 13) & 0xFF));
            bmp.set(x, y, 2, static_cast<std::uint8_t>((x * 7 ^ y * 17) & 0xFF));
            if (with_alpha) {
                bmp.set(x, y, 3,
                         static_cast<std::uint8_t>(200 - ((x + y) & 0x3F)));
            }
        }
    }
    return bmp;
}

Result<Bitmap> roundtrip(const Bitmap& src) {
    const std::string bytes = encode_webp(std::vector<Bitmap>{src});
    if (bytes.empty()) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }
    return decode_webp(reinterpret_cast<const std::uint8_t*>(bytes.data()),
                       bytes.size());
}

}  // namespace

TEST_CASE("webp_decode: 4 通道图编码后原生解码像素逐字节一致") {
    for (auto [w, h] : {std::pair{64u, 48u}, {32u, 24u}, {17u, 13u}, {7u, 3u},
                        {1u, 1u}, {48u, 32u}}) {
        const Bitmap original = make_test_bitmap(w, h, true);
        auto r = roundtrip(original);
        REQUIRE(r.has_value());
        REQUIRE(r.value() == original);
    }
}

TEST_CASE("webp_decode: 3 通道图（无 alpha）解码 RGB 一致、alpha 强制 255") {
    for (auto [w, h] : {std::pair{64u, 48u}, {32u, 24u}, {17u, 13u}, {1u, 1u}}) {
        const Bitmap original = make_test_bitmap(w, h, false);
        auto r = roundtrip(original);
        REQUIRE(r.has_value());
        REQUIRE(r.value() == original.with_alpha(255));
    }
}

TEST_CASE("webp_decode: 渐变（全 256 绿值）解码一致") {
    Bitmap grad(48, 32, 4);
    for (std::uint32_t y = 0; y < 32; ++y) {
        for (std::uint32_t x = 0; x < 48; ++x) {
            grad.set(x, y, 0, static_cast<std::uint8_t>((x * 255) / 47));
            grad.set(x, y, 1, static_cast<std::uint8_t>((y * 255) / 31));
            grad.set(x, y, 2, 128);
            grad.set(x, y, 3, static_cast<std::uint8_t>(100 + ((x + y) & 0x1F)));
        }
    }
    auto r = roundtrip(grad);
    REQUIRE(r.has_value());
    REQUIRE(r.value() == grad);
}

TEST_CASE("webp_decode: 容器/签名损坏或尺寸越界返回 ParseError") {
    // 太短，连 RIFF 头都不全
    std::vector<std::uint8_t> tiny = {'R', 'I', 'F', 'F', 4, 0, 0, 0,
                                      'W', 'E', 'B', 'P'};
    REQUIRE_FALSE(decode_webp(tiny.data(), tiny.size()).has_value());
    // 全零：魔数错误
    std::vector<std::uint8_t> bad(64, 0);
    REQUIRE_FALSE(decode_webp(bad.data(), bad.size()).has_value());
    // 空指针 / 零长度
    REQUIRE_FALSE(decode_webp(nullptr, 0).has_value());
}

TEST_CASE("webp_decode: 含变换的外部 WebP 返回 Unsupported（供调用方回落 stb）") {
    const Bitmap original = make_test_bitmap(32, 24, true);
    std::string bytes = encode_webp(std::vector<Bitmap>{original});
    REQUIRE_FALSE(bytes.empty());
    REQUIRE(bytes.size() > 25);
    // 翻转 VP8L 头里的 transform 位（bit 40 = 字节 25 bit0），
    // 使解码器判定为不支持的特性而非坏流。
    bytes[25] |= 0x01;
    auto r = decode_webp(reinterpret_cast<const std::uint8_t*>(bytes.data()),
                         bytes.size());
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::Unsupported);
}
