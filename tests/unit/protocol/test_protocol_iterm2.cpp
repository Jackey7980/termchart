// test_protocol_iterm2.cpp —— iTerm2 内联图像（OSC 1337 + PNG + base64）
#include <doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/png_encode.hpp"
#include "termchart/protocol/iterm2_inline.hpp"

namespace {

using namespace termchart;
using namespace termchart::protocol;

// 逐字节按**无符号**比较：char 在 MSVC 下有符号，直接比 0x89 会被提升成 -119。
bool has_png_signature(const std::string& bytes) {
    static constexpr std::uint8_t kSignature[8] = {0x89, 'P', 'N', 'G',
                                                  0x0D, 0x0A, 0x1A, 0x0A};
    if (bytes.size() < 8) {
        return false;
    }
    for (std::size_t i = 0; i < 8; ++i) {
        if (static_cast<std::uint8_t>(bytes[i]) != kSignature[i]) {
            return false;
        }
    }
    return true;
}

// 测试自带的 base64 解码（不复用被测代码里的编码器，避免同错同过）。
bool base64_decode(const std::string& text, std::string& out) {
    static constexpr char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int accum = 0;
    int bits = 0;
    out.clear();
    for (const char ch : text) {
        if (ch == '=') {
            break;
        }
        const char* found = std::char_traits<char>::find(kTable, 64, ch);
        if (found == nullptr) {
            return false;
        }
        accum = (accum << 6) | static_cast<int>(found - kTable);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((accum >> bits) & 0xFF));
        }
    }
    return true;
}

// 从 OSC 序列里取出 ": " 之后的 base64 载荷。
bool payload_of(const std::string& sequence, std::string& payload) {
    const std::size_t colon = sequence.find(':');
    if (colon == std::string::npos || sequence.empty() || sequence.back() != '\a') {
        return false;
    }
    payload = sequence.substr(colon + 1, sequence.size() - colon - 2);
    return true;
}

image::Bitmap probe_bitmap(std::uint32_t w, std::uint32_t h) {
    image::Bitmap bmp(w, h, 4);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            bmp.set(x, y, 0, static_cast<std::uint8_t>(x * 9));
            bmp.set(x, y, 1, static_cast<std::uint8_t>(y * 5));
            bmp.set(x, y, 2, 0x7F);
            bmp.set(x, y, 3, 0xFF);
        }
    }
    return bmp;
}

}  // namespace

TEST_CASE("protocol/iterm2: 序列形状是 OSC 1337 File=… : <base64> BEL") {
    const image::Bitmap bmp = probe_bitmap(4, 3);
    Iterm2Inline protocol;
    const std::string s = protocol.encode(bmp);

    CHECK(s.rfind("\x1b]1337;File=", 0) == 0);  // 以 OSC 1337 开头
    CHECK(s.find("inline=1") != std::string::npos);
    CHECK(s.find("width=4px") != std::string::npos);
    CHECK(s.find("height=3px") != std::string::npos);
    CHECK(s.back() == '\a');  // BEL 结束
    CHECK(s.find("\x1b\\") == std::string::npos);  // 不用 ST 形式

    // size= 声明的字节数必须等于载荷解出来的 PNG 长度。
    const std::size_t size_at = s.find("size=");
    REQUIRE(size_at != std::string::npos);
    const std::size_t size_end = s.find(';', size_at);
    REQUIRE(size_end != std::string::npos);
    const auto declared = static_cast<std::size_t>(
        std::stoul(s.substr(size_at + 5, size_end - size_at - 5)));

    std::string payload;
    REQUIRE(payload_of(s, payload));
    std::string png;
    REQUIRE(base64_decode(payload, png));
    CHECK(png.size() == declared);
    REQUIRE(png.size() > 8);
    CHECK(has_png_signature(png));

    // 载荷与内联 PNG 编码器的输出完全一致（同一张图没有第二种编码）。
    CHECK(png == image::encode_png(bmp));
}

TEST_CASE("protocol/iterm2: 空位图返回空串") {
    Iterm2Inline protocol;
    CHECK(protocol.encode(image::Bitmap{}).empty());
    CHECK(protocol.encode(image::Bitmap(2, 0, 4)).empty());
}

TEST_CASE("protocol/iterm2: 名字与 kind") {
    Iterm2Inline protocol;
    CHECK(protocol.kind() == ProtocolKind::Iterm2);
    CHECK(std::string(protocol.name()) == "iterm2");
    CHECK(std::string(protocol_name(ProtocolKind::Iterm2)) == "iterm2");
}
