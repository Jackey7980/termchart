// test_png_encode.cpp —— PNG 编码器
//
// 验证策略：不重写一遍同样的算法（那样同错同过），而是
//   1) 用**标准校验值**钉死 CRC-32 / Adler-32；
//   2) 用一个**独立的最小解码器**把字节流解回像素，与原位图逐字节比对；
//   3) 逐 chunk 重算 CRC 与长度，确保结构合规。
#include <doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/image/bitmap.hpp"
#include "termchart/image/png_encode.hpp"

namespace {

using namespace termchart;
using namespace termchart::image;

// 逐字节按**无符号**比较：char 在 MSVC 下有符号，直接比 0x89 会被提升成 -119。
bool has_png_signature(const std::string& png) {
    static constexpr std::uint8_t kSignature[8] = {0x89, 'P', 'N', 'G',
                                                  0x0D, 0x0A, 0x1A, 0x0A};
    if (png.size() < 8) {
        return false;
    }
    for (std::size_t i = 0; i < 8; ++i) {
        if (static_cast<std::uint8_t>(png[i]) != kSignature[i]) {
            return false;
        }
    }
    return true;
}

std::uint32_t read_u32_be(const std::string& s, std::size_t at) {
    return (static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[at])) << 24) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[at + 1])) << 16) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[at + 2])) << 8) |
           static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[at + 3]));
}

struct Chunk {
    std::string type;
    std::string data;
    bool crc_matches = false;
};

// 遍历 PNG chunk；遇到结构非法直接返回已解析部分（测试断言数量即可发现）。
std::vector<Chunk> parse_chunks(const std::string& png) {
    std::vector<Chunk> chunks;
    std::size_t at = 8;  // 跳过签名
    while (at + 12 <= png.size()) {
        const std::uint32_t length = read_u32_be(png, at);
        if (at + 12 + length > png.size()) {
            break;
        }
        Chunk chunk;
        chunk.type = png.substr(at + 4, 4);
        chunk.data = png.substr(at + 8, length);
        const std::uint32_t stored_crc = read_u32_be(png, at + 8 + length);
        const auto* crc_bytes =
            reinterpret_cast<const std::uint8_t*>(png.data() + at + 4);
        chunk.crc_matches = crc32(crc_bytes, 4 + length) == stored_crc;
        chunks.push_back(chunk);
        at += 12 + length;
    }
    return chunks;
}

// 最小 zlib 解码器：只认 stored（未压缩）deflate 块 —— 正是编码器的输出形式。
// 返回 false 表示遇到了不认识的块类型（那说明编码器换了实现，测试需要同步）。
bool inflate_stored(const std::string& zlib, std::string& raw) {
    raw.clear();
    if (zlib.size() < 6) {
        return false;
    }
    std::size_t at = 2;  // zlib 头
    while (at + 5 <= zlib.size()) {
        const auto header = static_cast<std::uint8_t>(zlib[at]);
        if ((header & 0x06u) != 0) {
            return false;  // 非 stored 块
        }
        const auto len = static_cast<std::uint16_t>(
            static_cast<std::uint8_t>(zlib[at + 1]) |
            (static_cast<std::uint16_t>(static_cast<std::uint8_t>(zlib[at + 2])) << 8));
        const auto nlen = static_cast<std::uint16_t>(
            static_cast<std::uint8_t>(zlib[at + 3]) |
            (static_cast<std::uint16_t>(static_cast<std::uint8_t>(zlib[at + 4])) << 8));
        if (static_cast<std::uint16_t>(~len) != nlen) {
            return false;
        }
        if (at + 5 + len > zlib.size()) {
            return false;
        }
        raw.append(zlib, at + 5, len);
        at += 5 + len;
        if ((header & 0x01u) != 0) {
            break;  // BFINAL
        }
    }
    // 尾部 4 字节是 Adler-32（大端）
    const std::uint32_t stored = read_u32_be(zlib, zlib.size() - 4);
    return stored ==
           adler32(reinterpret_cast<const std::uint8_t*>(raw.data()), raw.size());
}

// 由解码出的扫描线还原像素（编码器只用 filter 0）。
bool decode_scanlines(const std::string& raw, Bitmap& out) {
    const std::size_t stride = out.row_bytes();
    if (raw.size() != (stride + 1) * out.height()) {
        return false;
    }
    for (std::size_t y = 0; y < out.height(); ++y) {
        if (raw[y * (stride + 1)] != '\0') {
            return false;
        }
    }
    for (std::size_t y = 0; y < out.height(); ++y) {
        const char* line = raw.data() + y * (stride + 1) + 1;
        std::uint8_t* dst = out.data() + y * stride;
        for (std::size_t i = 0; i < stride; ++i) {
            dst[i] = static_cast<std::uint8_t>(line[i]);
        }
    }
    return true;
}

Bitmap make_bitmap(std::uint32_t w, std::uint32_t h, std::uint8_t channels) {
    Bitmap bmp(w, h, channels);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            bmp.set(x, y, 0, static_cast<std::uint8_t>(x * 7 + y));
            bmp.set(x, y, 1, static_cast<std::uint8_t>(x ^ y));
            bmp.set(x, y, 2, static_cast<std::uint8_t>(255 - ((x + y) & 0xFF)));
            if (channels == 4) {
                bmp.set(x, y, 3, static_cast<std::uint8_t>(200 - (x & 0x3F)));
            }
        }
    }
    return bmp;
}

}  // namespace

TEST_CASE("image/png: CRC-32 与 Adler-32 对上标准校验值") {
    const std::string probe = "123456789";  // 两个算法的标准测试向量
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(probe.data());
    CHECK(crc32(bytes, probe.size()) == 0xCBF43926u);
    CHECK(adler32(bytes, probe.size()) == 0x091E01DEu);

    CHECK(crc32(nullptr, 0) == 0u);
    CHECK(adler32(nullptr, 0) == 1u);

    const std::string wiki = "Wikipedia";
    CHECK(adler32(reinterpret_cast<const std::uint8_t*>(wiki.data()), wiki.size()) ==
          0x11E60398u);
}

TEST_CASE("image/png: 头部与 chunk 结构合规") {
    const Bitmap bmp = make_bitmap(3, 2, 4);
    const std::string png = encode_png(bmp);

    REQUIRE(png.size() > 8);
    CHECK(has_png_signature(png));

    const std::vector<Chunk> chunks = parse_chunks(png);
    REQUIRE(chunks.size() == 3);
    CHECK(chunks[0].type == "IHDR");
    CHECK(chunks[1].type == "IDAT");
    CHECK(chunks[2].type == "IEND");
    for (const Chunk& chunk : chunks) {
        CHECK(chunk.crc_matches);  // 长度与 CRC 都必须对得上
    }

    REQUIRE(chunks[0].data.size() == 13);
    CHECK(read_u32_be(chunks[0].data, 0) == 3u);   // 宽
    CHECK(read_u32_be(chunks[0].data, 4) == 2u);   // 高
    CHECK(static_cast<std::uint8_t>(chunks[0].data[8]) == 8u);   // 位深
    CHECK(static_cast<std::uint8_t>(chunks[0].data[9]) == 6u);   // RGBA
    CHECK(chunks[2].data.empty());
}

TEST_CASE("image/png: RGB 位图的颜色类型是 2") {
    const Bitmap bmp = make_bitmap(2, 2, 3);
    const std::vector<Chunk> chunks = parse_chunks(encode_png(bmp));
    REQUIRE(chunks.size() == 3);
    CHECK(static_cast<std::uint8_t>(chunks[0].data[9]) == 2u);
}

TEST_CASE("image/png: 解回来与原位图逐字节一致（小图）") {
    const Bitmap bmp = make_bitmap(5, 4, 4);
    const std::vector<Chunk> chunks = parse_chunks(encode_png(bmp));
    REQUIRE(chunks.size() == 3);

    const std::string& idat = chunks[1].data;
    CHECK(static_cast<std::uint8_t>(idat[0]) == 0x78u);  // zlib CMF
    CHECK(static_cast<std::uint8_t>(idat[1]) == 0x01u);  // zlib FLG
    CHECK((static_cast<std::uint32_t>(static_cast<std::uint8_t>(idat[0])) * 256u +
           static_cast<std::uint8_t>(idat[1])) % 31u == 0u);  // 头部校验

    std::string raw;
    REQUIRE(inflate_stored(idat, raw));
    Bitmap decoded(5, 4, 4);
    REQUIRE(decode_scanlines(raw, decoded));
    CHECK(decoded == bmp);
}

TEST_CASE("image/png: 超过 64 KiB 时拆多个 stored 块，仍能解回") {
    // 200x120 RGBA = 96000 字节扫描线 > 65535 → 至少两个块。
    const Bitmap bmp = make_bitmap(200, 120, 4);
    const std::vector<Chunk> chunks = parse_chunks(encode_png(bmp));
    REQUIRE(chunks.size() == 3);

    const std::string& idat = chunks[1].data;
    std::string raw;
    REQUIRE(inflate_stored(idat, raw));
    CHECK(raw.size() == (200u * 4u + 1u) * 120u);

    Bitmap decoded(200, 120, 4);
    REQUIRE(decode_scanlines(raw, decoded));
    CHECK(decoded == bmp);
}

TEST_CASE("image/png: 空位图返回空串而不是非法文件") {
    CHECK(encode_png(Bitmap{}).empty());
    CHECK(encode_png(Bitmap(4, 0, 4)).empty());
    CHECK(encode_png(Bitmap(0, 4, 4)).empty());
}
