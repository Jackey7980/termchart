// png_encode.cpp —— 最小 PNG 编码器实现
#include "termchart/media/image/png_encode.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace termchart::image {
namespace {

// zlib 的经典分块长度：每 5552 字节取一次模，保证 uint32 累加不溢出。
constexpr std::size_t kAdlerChunk = 5552;
constexpr std::uint32_t kAdlerMod = 65521;
// deflate stored 块的 LEN 是 16 位，单块上限 65535 字节。
constexpr std::size_t kStoredBlockMax = 65535;

void put_u16_le(std::string& out, std::uint16_t value) {
    out.push_back(static_cast<char>(value & 0xFFu));
    out.push_back(static_cast<char>((value >> 8) & 0xFFu));
}

void put_u32_be(std::string& out, std::uint32_t value) {
    out.push_back(static_cast<char>((value >> 24) & 0xFFu));
    out.push_back(static_cast<char>((value >> 16) & 0xFFu));
    out.push_back(static_cast<char>((value >> 8) & 0xFFu));
    out.push_back(static_cast<char>(value & 0xFFu));
}

// chunk = 长度(4, BE) + 类型(4) + 数据 + CRC(4, BE)；CRC 覆盖"类型 + 数据"。
void append_chunk(std::string& out, const char type[4], const std::string& data) {
    put_u32_be(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t crc_begin = out.size();
    out.append(type, 4);
    out += data;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(out.data() + crc_begin);
    put_u32_be(out, crc32(bytes, 4 + data.size()));
}

// 原始扫描线：每行前置一个 filter 字节（恒为 0 = None，配合 stored 块无需预测滤波）。
std::string raw_scanlines(const Bitmap& bitmap) {
    const std::size_t stride = bitmap.row_bytes();
    const std::size_t rows = bitmap.height();
    std::string raw;
    raw.reserve((stride + 1) * rows);
    for (std::size_t y = 0; y < rows; ++y) {
        raw.push_back('\0');
        const char* line = reinterpret_cast<const char*>(bitmap.data()) + y * stride;
        raw.append(line, stride);
    }
    return raw;
}

// zlib 流：2 字节头（0x78 0x01 = deflate / 32K 窗口 / 无字典 / 最快档）
// + 若干个 stored deflate 块 + 4 字节 Adler-32（大端）。
std::string zlib_stream(const std::string& raw) {
    std::string out;
    out.reserve(raw.size() + raw.size() / 1024 + 32);
    out.push_back('\x78');
    out.push_back('\x01');

    std::size_t offset = 0;
    do {
        const std::size_t count = std::min(kStoredBlockMax, raw.size() - offset);
        const bool final_block = offset + count >= raw.size();
        // 块头 3 位（BFINAL + BTYPE=00）按 LSB 优先打包，故字节值即 0 或 1。
        out.push_back(final_block ? '\x01' : '\x00');
        put_u16_le(out, static_cast<std::uint16_t>(count));
        put_u16_le(out, static_cast<std::uint16_t>(~count));  // NLEN = ~LEN
        out.append(raw, offset, count);
        offset += count;
    } while (offset < raw.size());

    put_u32_be(out, adler32(reinterpret_cast<const std::uint8_t*>(raw.data()), raw.size()));
    return out;
}

}  // namespace

std::uint32_t crc32(const std::uint8_t* data, std::size_t len) noexcept {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            // 反射多项式 0xEDB88320：(crc & 1) ? (crc >> 1) ^ poly : (crc >> 1)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

std::uint32_t adler32(const std::uint8_t* data, std::size_t len) noexcept {
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    std::size_t offset = 0;
    while (offset < len) {
        const std::size_t count = std::min(kAdlerChunk, len - offset);
        for (std::size_t i = 0; i < count; ++i) {
            a += data[offset + i];
            b += a;
        }
        a %= kAdlerMod;
        b %= kAdlerMod;
        offset += count;
    }
    return (b << 16) | a;
}

std::string encode_png(const Bitmap& bitmap) {
    if (bitmap.width() == 0 || bitmap.height() == 0) {
        return {};
    }

    const std::string raw = raw_scanlines(bitmap);
    const std::string idat = zlib_stream(raw);

    std::string out;
    out.reserve(idat.size() + 64);
    // 8 字节 PNG 签名。
    out.append("\x89PNG\r\n\x1a\n", 8);

    // IHDR：宽(4) 高(4) 位深(1) 颜色类型(1) 压缩(1) 滤波(1) 隔行(1)。
    std::string ihdr;
    ihdr.reserve(13);
    put_u32_be(ihdr, bitmap.width());
    put_u32_be(ihdr, bitmap.height());
    ihdr.push_back('\x08');  // 位深 8
    ihdr.push_back(bitmap.channels() == 4 ? '\x06' : '\x02');  // RGBA / RGB
    ihdr.push_back('\x00');  // 压缩方法：deflate
    ihdr.push_back('\x00');  // 滤波方法：标准
    ihdr.push_back('\x00');  // 隔行：无
    append_chunk(out, "IHDR", ihdr);

    append_chunk(out, "IDAT", idat);
    append_chunk(out, "IEND", std::string{});
    return out;
}

}  // namespace termchart::image
