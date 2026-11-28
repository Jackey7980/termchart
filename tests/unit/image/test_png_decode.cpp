// test_png_decode.cpp —— PNG 解码器
//
// 两类验证：
//   1) **与本库编码器的往返**（滤波 0 + stored deflate 块）；
//   2) **与 Python 参考 zlib 的交叉验证**（test_png_samples.inc，覆盖滤波 1..4、
//      动态 Huffman、调色板 8/4 位、灰度 16 位、灰度+alpha、坏 CRC、Adam7）。
// 第 2 类才是真正在测 inflate 与反滤波——只靠自家编码器往返，这些分支根本没被走到。
#include <doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "test_png_samples.inc"
#include "termchart/core/error.hpp"
#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/decoder.hpp"
#include "termchart/media/image/png.hpp"
#include "termchart/media/image/png_encode.hpp"

namespace {

using namespace termchart;
using namespace termchart::image;

// 与 tools/gen_png_samples.py 中的公式逐字对应（改公式要两边一起改）。
std::uint8_t expected_rgb(std::uint32_t x, std::uint32_t y, int channel) {
    switch (channel) {
        case 0:
            return static_cast<std::uint8_t>((x * 37 + y * 11) % 256);
        case 1:
            return static_cast<std::uint8_t>((x * 17 + y * 53) % 256);
        default:
            return static_cast<std::uint8_t>((x * 97 + y * 7) % 256);
    }
}

std::uint8_t expected_gray(std::uint32_t x, std::uint32_t y) {
    return static_cast<std::uint8_t>((x * 40 + y * 20) % 256);
}

std::uint32_t expected_gray16(std::uint32_t x, std::uint32_t y) {
    return (x * 3000 + y * 500) % 65536;
}

std::uint8_t expected_palette_channel(std::uint32_t index, int channel) {
    switch (channel) {
        case 0:
            return static_cast<std::uint8_t>(index * 8);
        case 1:
            return static_cast<std::uint8_t>(255 - index * 8);
        default:
            return static_cast<std::uint8_t>(index * 4);
    }
}

Bitmap decode_or_fail(const unsigned char* data, std::size_t size) {
    const Result<Bitmap> result = decode_png(data, size);
    REQUIRE(result.has_value());
    return result.value();
}

void check_rgb_image(const Bitmap& bitmap, std::uint32_t width, std::uint32_t height) {
    REQUIRE(bitmap.width() == width);
    REQUIRE(bitmap.height() == height);
    REQUIRE(bitmap.channels() == 4);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            CHECK(bitmap.at(x, y, 0) == expected_rgb(x, y, 0));
            CHECK(bitmap.at(x, y, 1) == expected_rgb(x, y, 1));
            CHECK(bitmap.at(x, y, 2) == expected_rgb(x, y, 2));
            CHECK(bitmap.at(x, y, 3) == 255);
        }
    }
}

}  // namespace

TEST_CASE("image/png 解码: 四种滤波都能还原原始像素") {
    check_rgb_image(decode_or_fail(kSubPng, sizeof(kSubPng)), 5, 3);      // Sub
    check_rgb_image(decode_or_fail(kUpPng, sizeof(kUpPng)), 5, 3);        // Up
    check_rgb_image(decode_or_fail(kAveragePng, sizeof(kAveragePng)), 5, 3);  // Average
    check_rgb_image(decode_or_fail(kPaethPng, sizeof(kPaethPng)), 5, 3);  // Paeth
}

TEST_CASE("image/png 解码: 动态 Huffman 块（参考 zlib 压缩）") {
    const Bitmap bitmap = decode_or_fail(kDynamicPng, sizeof(kDynamicPng));
    REQUIRE(bitmap.width() == 24);
    REQUIRE(bitmap.height() == 8);
    for (std::uint32_t y = 0; y < 8; ++y) {
        for (std::uint32_t x = 0; x < 24; ++x) {
            CHECK(bitmap.at(x, y, 0) == static_cast<std::uint8_t>((x % 4) * 60));
            CHECK(bitmap.at(x, y, 1) == static_cast<std::uint8_t>((y % 3) * 80));
            CHECK(bitmap.at(x, y, 2) == 128);
            CHECK(bitmap.at(x, y, 3) == 255);
        }
    }
}

TEST_CASE("image/png 解码: 调色板 8 位与 tRNS 透明") {
    const Bitmap bitmap = decode_or_fail(kPalette8Png, sizeof(kPalette8Png));
    REQUIRE(bitmap.width() == 4);
    REQUIRE(bitmap.height() == 4);
    for (std::uint32_t y = 0; y < 4; ++y) {
        for (std::uint32_t x = 0; x < 4; ++x) {
            const std::uint32_t index = (x + y) % 6;
            CHECK(bitmap.at(x, y, 0) == expected_palette_channel(index, 0));
            CHECK(bitmap.at(x, y, 1) == expected_palette_channel(index, 1));
            CHECK(bitmap.at(x, y, 2) == expected_palette_channel(index, 2));
            // tRNS 给出前 6 个索引的 alpha
            const std::uint8_t alpha[6] = {255, 128, 0, 64, 32, 16};
            CHECK(bitmap.at(x, y, 3) == alpha[index]);
        }
    }
}

TEST_CASE("image/png 解码: 调色板 4 位（同一字节塞两个像素）") {
    const Bitmap bitmap = decode_or_fail(kPalette4Png, sizeof(kPalette4Png));
    REQUIRE(bitmap.width() == 8);
    REQUIRE(bitmap.height() == 2);
    for (std::uint32_t y = 0; y < 2; ++y) {
        for (std::uint32_t x = 0; x < 8; ++x) {
            const std::uint32_t index = (x + y) % 6;
            CHECK(bitmap.at(x, y, 0) == expected_palette_channel(index, 0));
            CHECK(bitmap.at(x, y, 3) == 255);  // 没给 tRNS：不透明
        }
    }
}

TEST_CASE("image/png 解码: 灰度 16 位取高字节") {
    const Bitmap bitmap = decode_or_fail(kGray16Png, sizeof(kGray16Png));
    REQUIRE(bitmap.width() == 4);
    REQUIRE(bitmap.height() == 2);
    for (std::uint32_t y = 0; y < 2; ++y) {
        for (std::uint32_t x = 0; x < 4; ++x) {
            const auto expected =
                static_cast<std::uint8_t>(expected_gray16(x, y) >> 8);
            CHECK(bitmap.at(x, y, 0) == expected);
            CHECK(bitmap.at(x, y, 1) == expected);
            CHECK(bitmap.at(x, y, 2) == expected);
            CHECK(bitmap.at(x, y, 3) == 255);
        }
    }
}

TEST_CASE("image/png 解码: 灰度+alpha 与真彩+alpha") {
    const Bitmap gray_alpha = decode_or_fail(kGrayAlphaPng, sizeof(kGrayAlphaPng));
    REQUIRE(gray_alpha.width() == 4);
    REQUIRE(gray_alpha.height() == 2);
    for (std::uint32_t y = 0; y < 2; ++y) {
        for (std::uint32_t x = 0; x < 4; ++x) {
            CHECK(gray_alpha.at(x, y, 0) == expected_gray(x, y));
            CHECK(gray_alpha.at(x, y, 3) == 255 - expected_gray(x, y));
        }
    }

    const Bitmap rgba = decode_or_fail(kRgbaPng, sizeof(kRgbaPng));
    REQUIRE(rgba.width() == 4);
    REQUIRE(rgba.height() == 2);
    for (std::uint32_t y = 0; y < 2; ++y) {
        for (std::uint32_t x = 0; x < 4; ++x) {
            CHECK(rgba.at(x, y, 0) == expected_rgb(x, y, 0));
            CHECK(rgba.at(x, y, 2) == expected_rgb(x, y, 2));
            CHECK(rgba.at(x, y, 3) == static_cast<std::uint8_t>((x * 60 + y * 30) % 256));
        }
    }
}

TEST_CASE("image/png 解码: 坏 CRC 与 Adam7 隔行都要如实报错") {
    const Result<Bitmap> bad_crc = decode_png(kBadCrcPng, sizeof(kBadCrcPng));
    REQUIRE_FALSE(bad_crc.has_value());
    CHECK(bad_crc.error() == ErrorCode::ParseError);

    const Result<Bitmap> interlaced = decode_png(kInterlacedPng, sizeof(kInterlacedPng));
    REQUIRE_FALSE(interlaced.has_value());
    CHECK(interlaced.error() == ErrorCode::Unsupported);
}

TEST_CASE("image/png 解码: 与本库编码器往返（RGB 与 RGBA）") {
    // RGBA：逐字节一致（编码器写 4 通道，解码器也出 4 通道）
    Bitmap source(7, 5, 4);
    for (std::uint32_t y = 0; y < 5; ++y) {
        for (std::uint32_t x = 0; x < 7; ++x) {
            source.set(x, y, 0, static_cast<std::uint8_t>(x * 30));
            source.set(x, y, 1, static_cast<std::uint8_t>(y * 50));
            source.set(x, y, 2, 0x7F);
            source.set(x, y, 3, static_cast<std::uint8_t>(255 - x * 20));
        }
    }
    const std::string encoded = encode_png(source);
    const Bitmap decoded = decode_or_fail(
        reinterpret_cast<const unsigned char*>(encoded.data()), encoded.size());
    CHECK(decoded == source);

    // RGB：解码出来是 RGBA，颜色一致且 alpha = 255
    const Bitmap rgb_source = source.without_alpha();
    const std::string rgb_encoded = encode_png(rgb_source);
    const Bitmap rgb_decoded = decode_or_fail(
        reinterpret_cast<const unsigned char*>(rgb_encoded.data()), rgb_encoded.size());
    REQUIRE(rgb_decoded.width() == 7);
    REQUIRE(rgb_decoded.channels() == 4);
    for (std::uint32_t y = 0; y < 5; ++y) {
        for (std::uint32_t x = 0; x < 7; ++x) {
            CHECK(rgb_decoded.at(x, y, 0) == rgb_source.at(x, y, 0));
            CHECK(rgb_decoded.at(x, y, 3) == 255);
        }
    }
}

TEST_CASE("image/png 解码: 畸形输入一律失败，不产出半张图") {
    CHECK_FALSE(decode_png(nullptr, 0).has_value());
    CHECK_FALSE(decode_png(std::string_view{}).has_value());

    const std::string not_png = "这不是 PNG";
    const Result<Bitmap> result = decode_png(not_png);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ParseError);

    // 截断：只保留签名与 IHDR 的一部分
    const std::string truncated(kSubPng, kSubPng + 20);
    CHECK_FALSE(decode_png(truncated).has_value());

    // 签名正确但 IDAT 被砍掉
    std::string no_idat(kSubPng, kSubPng + 8);
    CHECK_FALSE(decode_png(no_idat).has_value());
}

TEST_CASE("image/png 解码: 宽高撒谎的 IHDR 被尺寸守卫拒绝，而不是照着去分配") {
    // 造一个"签名 + IHDR(65535×65535) + IEND"的最小 PNG，**所有块 CRC 都算对**——
    // 否则会在 CRC 那一步就被拦下，根本测不到维度守卫。
    // 注意 IEND 不能省：解码器先跑 chunk 解析，缺 IEND 会直接判 ParseError，
    // 于是这条用例就变成"在测缺 IEND"了（fuzz 的同类种子第一版就栽在这里）。
    const auto be32 = [](std::uint32_t value) {
        std::string out;
        for (int shift = 24; shift >= 0; shift -= 8) {
            out.push_back(static_cast<char>((value >> shift) & 0xFFu));
        }
        return out;
    };
    const auto chunk = [&be32](const std::string& type, const std::string& payload) {
        const std::string body = type + payload;
        return be32(static_cast<std::uint32_t>(payload.size())) + body +
               be32(termchart::image::crc32(
                   reinterpret_cast<const std::uint8_t*>(body.data()), body.size()));
    };

    std::string ihdr = be32(0xFFFF) + be32(0xFFFF);
    ihdr.push_back(8);            // bit depth
    ihdr.push_back(6);            // color type: RGBA
    ihdr += std::string(3, '\0'); // compression / filter / interlace
    std::string png("\x89PNG\r\n\x1a\n", 8);
    png += chunk("IHDR", ihdr);
    png += chunk("IEND", "");

    const Result<Bitmap> result = decode_png(png);
    REQUIRE_FALSE(result.has_value());
    // 必须是 **OutOfRange**：说明解码器走到了维度检查那一步并主动拒绝，
    // 没有照着 4G 像素去分配内存（这正是该守卫存在的意义）。
    CHECK(result.error() == ErrorCode::OutOfRange);
}

TEST_CASE("image/decoder: 按魔数识别格式并分发") {
    CHECK(sniff_image_format(kSubPng, sizeof(kSubPng)) == DetectedFormat::Png);
    const unsigned char jpeg[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10};
    CHECK(sniff_image_format(jpeg, sizeof(jpeg)) == DetectedFormat::Jpeg);
    const unsigned char gif[] = {'G', 'I', 'F', '8', '9', 'a'};
    CHECK(sniff_image_format(gif, sizeof(gif)) == DetectedFormat::Gif);
    const unsigned char bmp[] = {'B', 'M', 0x36, 0x00};
    CHECK(sniff_image_format(bmp, sizeof(bmp)) == DetectedFormat::Bmp);
    const unsigned char webp[] = {'R', 'I', 'F', 'F', 0x24, 0x00, 0x00, 0x00,
                                  'W', 'E', 'B', 'P'};
    CHECK(sniff_image_format(webp, sizeof(webp)) == DetectedFormat::WebP);
    const unsigned char junk[] = {1, 2, 3, 4};
    CHECK(sniff_image_format(junk, sizeof(junk)) == DetectedFormat::Unknown);
    CHECK(sniff_image_format(nullptr, 0) == DetectedFormat::Unknown);

    CHECK(std::string(image_format_name(DetectedFormat::Png)) == "png");
    CHECK(std::string(image_format_name(DetectedFormat::WebP)) == "webp");

    // PNG 有解码器：直接解
    const Result<Bitmap> png = decode_image(kPalette8Png, sizeof(kPalette8Png));
    REQUIRE(png.has_value());
    CHECK(png.value().width() == 4);

    // 认得出但没有解码器：Unsupported（不是 ParseError）
    const Result<Bitmap> unsupported = decode_image(jpeg, sizeof(jpeg));
    REQUIRE_FALSE(unsupported.has_value());
#ifndef TERMCHART_HAVE_STB_IMAGE
    CHECK(unsupported.error() == ErrorCode::Unsupported);
#else
    // 启用 stb 后 JPG 认得出且有解码器，只是这段 JPEG 魔数不是合法图像体，
    // 因此走到 stb 解码失败 → ParseError（不再是 Unsupported）。
    CHECK(unsupported.error() == ErrorCode::ParseError);
#endif

    // 认不出：ParseError
    const Result<Bitmap> unknown = decode_image(junk, sizeof(junk));
    REQUIRE_FALSE(unknown.has_value());
    CHECK(unknown.error() == ErrorCode::ParseError);
}
