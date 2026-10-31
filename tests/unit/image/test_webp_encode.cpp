// test_webp_encode.cpp —— 自包含 VP8L WebP 编码器测试
//
// 验证策略（与 test_png_encode.cpp 同哲学：不重写一遍同样的算法）：
//   1) 断言容器 / 签名 / 头部字段与入口语义（零外部依赖）；
//   2) 用一个**独立的最小 VP8L 解码器**把字节流解回像素，与原位图逐字节比对；
//      解码器自行从码长复算 canonical 码，不信任编码器的码字分配；
//   3) 落盘 tc_webp*.webp 供外部 ffmpeg / PIL 交叉校验：
//        ffmpeg -v error -i tc_webp.webp -f null -
//        ffprobe -show_entries stream=codec_name,width,height tc_webp.webp
#include <doctest.h>

#include <array>
#include <cstdio>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "termchart/image/bitmap.hpp"
#include "termchart/image/encode.hpp"
#include "termchart/image/webp_encode.hpp"

using namespace termchart;
using namespace termchart::image;

namespace {

// ---------------------------------------------------------------------------
// 最小独立 VP8L 解码器（仅用于回环校验，不进入库）
// ---------------------------------------------------------------------------

// 码长码位置顺序（与编码器、规范一致）。
constexpr int kCodeLengthCodeOrder[19] = {17, 18, 0, 1, 2, 3, 4, 5, 16, 6,
                                          7, 8, 9, 10, 11, 12, 13, 14, 15};

inline std::uint32_t reverse_bits(std::uint32_t c, int ln) {
    std::uint32_t r = 0;
    for (int i = 0; i < ln; ++i) {
        r = (r << 1) | ((c >> i) & 1u);
    }
    return r;
}

// 标准 canonical 码分配（与编码器同算法，但独立复算）。
std::vector<std::pair<std::uint32_t, int>> canonical_codes(const std::vector<int>& lengths) {
    int counts[16] = {0};
    for (int L : lengths) {
        if (L > 0) {
            ++counts[L];
        }
    }
    std::uint32_t next_code[16] = {0};
    std::uint32_t code = 0;
    for (int L = 1; L < 16; ++L) {
        next_code[L] = code;
        code = (code + static_cast<std::uint32_t>(counts[L])) << 1;
    }
    std::vector<std::pair<std::uint32_t, int>> codes(lengths.size());
    for (size_t s = 0; s < lengths.size(); ++s) {
        const int L = lengths[s];
        if (L == 0) {
            codes[s] = {0, 0};
        } else {
            codes[s] = {next_code[L], L};
            ++next_code[L];
        }
    }
    return codes;
}

// 由码长构建解码表：key = (长度<<16) | reverse(码值, 长度) -> 符号。
// 解码器按 LSB-first 累积 window，命中 reverse(码值) 即匹配（与 libwebp 一致）。
struct Huffman {
    std::unordered_map<std::uint32_t, int> lut;
};

Huffman build_huffman(const std::vector<int>& lengths) {
    Huffman h;
    const auto codes = canonical_codes(lengths);
    for (size_t s = 0; s < lengths.size(); ++s) {
        if (lengths[s] == 0) {
            continue;
        }
        const std::uint32_t rev = reverse_bits(codes[s].first, lengths[s]);
        h.lut[((std::uint32_t)lengths[s] << 16) | rev] = static_cast<int>(s);
    }
    return h;
}

struct BitReader {
    const std::string& s;
    std::size_t byte_pos;
    int bit_pos;

    explicit BitReader(const std::string& data, std::size_t start = 0)
        : s(data), byte_pos(start), bit_pos(0) {}

    int read_bit() {
        if (byte_pos >= s.size()) {
            return 0;  // 越界视作 0（测试里意味着坏流）
        }
        const int b = (static_cast<unsigned char>(s[byte_pos]) >> bit_pos) & 1;
        ++bit_pos;
        if (bit_pos == 8) {
            bit_pos = 0;
            ++byte_pos;
        }
        return b;
    }

    std::uint32_t read_bits(int n) {
        std::uint32_t v = 0;
        for (int i = 0; i < n; ++i) {
            v |= static_cast<std::uint32_t>(read_bit()) << i;
        }
        return v;
    }
};

// 在 h 中解码一个符号：LSB-first 累积，命中即返回。
bool decode_symbol(const Huffman& h, BitReader& br, int& out_sym) {
    std::uint32_t window = 0;
    int len = 0;
    for (;;) {
        window |= static_cast<std::uint32_t>(br.read_bit()) << len;
        ++len;
        auto it = h.lut.find(((std::uint32_t)len << 16) | window);
        if (it != h.lut.end()) {
            out_sym = it->second;
            return true;
        }
        if (len > 15) {
            return false;
        }
    }
}

// 读一棵树（非 simple 路径）：返回码长数组（长度 = alphabet_size）。
bool read_tree(BitReader& br, int alphabet_size, std::vector<int>& out_lengths) {
    const int simple = static_cast<int>(br.read_bits(1));
    if (simple != 0) {
        return false;  // 本编码器只产出非 simple 树
    }
    const int num_codes = static_cast<int>(br.read_bits(4)) + 4;
    if (num_codes > 19) {
        return false;
    }
    int clcc_len[19] = {0};
    for (int i = 0; i < num_codes; ++i) {
        const int idx = kCodeLengthCodeOrder[i];
        clcc_len[idx] = static_cast<int>(br.read_bits(3));
    }
    const int use_length = static_cast<int>(br.read_bits(1));
    if (use_length != 0) {
        return false;
    }
    // 码长码本身：4 个符号（位置 0/1/8/9）长度 2。
    const Huffman clcc = build_huffman(std::vector<int>(clcc_len, clcc_len + 19));
    out_lengths.assign(alphabet_size, 0);
    for (int i = 0; i < alphabet_size; ++i) {
        int sym;
        if (!decode_symbol(clcc, br, sym)) {
            return false;
        }
        // 码长码符号的取值即码长（位置 == 取值；本编码器只用 0/1/8/9）。
        if (sym != 0 && sym != 1 && sym != 8 && sym != 9) {
            return false;
        }
        out_lengths[i] = sym;
    }
    return true;
}

// 解码整个 WebP 回 RGBA Bitmap。失败返回 false。
bool decode_webp_to_rgba(const std::string& bytes, Bitmap& out) {
    if (bytes.size() < 20) {
        return false;
    }
    if (bytes.compare(0, 4, "RIFF") != 0) return false;
    if (bytes.compare(8, 4, "WEBP") != 0) return false;
    if (bytes.compare(12, 4, "VP8L") != 0) return false;
    const std::uint32_t chunk_size =
        static_cast<unsigned char>(bytes[16]) |
        (static_cast<unsigned char>(bytes[17]) << 8) |
        (static_cast<unsigned char>(bytes[18]) << 16) |
        (static_cast<unsigned char>(bytes[19]) << 24);
    if (chunk_size + 20 > bytes.size()) {
        return false;
    }
    const std::string vp8l = bytes.substr(20, chunk_size);
    BitReader br(vp8l, 0);

    if (br.read_bits(8) != 0x2Fu) return false;          // 签名
    const std::uint32_t w = br.read_bits(14) + 1;
    const std::uint32_t h = br.read_bits(14) + 1;
    const bool has_alpha = br.read_bits(1) != 0;
    (void)br.read_bits(3);                                // version
    if (br.read_bits(1) != 0) return false;               // 无变换
    if (br.read_bits(1) != 0) return false;               // 无色彩缓存
    if (br.read_bits(1) != 0) return false;               // 无元霍夫曼（单一 htree group）

    // 5 棵树：GREEN(280) / RED(256) / BLUE(256) / ALPHA(256) / DIST(40)。
    constexpr int kAlpha[5] = {280, 256, 256, 256, 40};
    Huffman trees[5];
    std::vector<int> lengths;
    for (int t = 0; t < 5; ++t) {
        if (!read_tree(br, kAlpha[t], lengths)) return false;
        trees[t] = build_huffman(lengths);
    }

    out = Bitmap(w, h, 4);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            int g, r, b, a;
            if (!decode_symbol(trees[0], br, g)) return false;
            if (!decode_symbol(trees[1], br, r)) return false;
            if (!decode_symbol(trees[2], br, b)) return false;
            if (!decode_symbol(trees[3], br, a)) return false;
            if (!has_alpha) {
                a = 255;  // 解码器据头部 alpha 位强制输出 255
            }
            out.set(x, y, 0, static_cast<std::uint8_t>(r));
            out.set(x, y, 1, static_cast<std::uint8_t>(g));
            out.set(x, y, 2, static_cast<std::uint8_t>(b));
            out.set(x, y, 3, static_cast<std::uint8_t>(a));
        }
    }
    return true;
}

Bitmap make_bitmap(std::uint32_t w, std::uint32_t h, bool with_alpha) {
    Bitmap bmp(w, h, with_alpha ? 4u : 3u);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            bmp.set(x, y, 0, static_cast<std::uint8_t>((x * 11 + y * 3) & 0xFF));
            bmp.set(x, y, 1, static_cast<std::uint8_t>((x * 5 + y * 13) & 0xFF));
            bmp.set(x, y, 2, static_cast<std::uint8_t>((x * 7 ^ y * 17) & 0xFF));
            if (with_alpha) {
                bmp.set(x, y, 3, static_cast<std::uint8_t>(200 - ((x + y) & 0x3F)));
            }
        }
    }
    return bmp;
}

bool write_file(const char* path, const std::string& data) {
    std::FILE* fp = std::fopen(path, "wb");
    if (fp == nullptr) return false;
    const bool ok = std::fwrite(data.data(), 1, data.size(), fp) == data.size();
    std::fclose(fp);
    return ok;
}

}  // namespace

// ---------------------------------------------------------------------------
// 容器 / 签名 / 入口语义（零外部依赖）
// ---------------------------------------------------------------------------

TEST_CASE("webp: RIFF/WEBP/VP8L 容器与 VP8L 签名") {
    Bitmap bm = make_bitmap(32, 24, true);
    const std::string bytes = encode(bm, ImageFormat::Webp);
    REQUIRE_FALSE(bytes.empty());
    REQUIRE(bytes.compare(0, 4, "RIFF") == 0);
    REQUIRE(bytes.compare(8, 4, "WEBP") == 0);
    REQUIRE(bytes.compare(12, 4, "VP8L") == 0);
    REQUIRE(static_cast<unsigned char>(bytes[20]) == 0x2F);  // VP8L 签名
    const std::uint32_t chunk_size = static_cast<unsigned char>(bytes[16]) |
                                     (static_cast<unsigned char>(bytes[17]) << 8) |
                                     (static_cast<unsigned char>(bytes[18]) << 16) |
                                     (static_cast<unsigned char>(bytes[19]) << 24);
    // RIFF 规定奇数长度 chunk 后补 1 个 pad 字节：不计入 chunk_size，但计入总长。
    const std::size_t pad = chunk_size & 1u;
    REQUIRE(bytes.size() == 20u + chunk_size + pad);
    const std::uint32_t riff_size = static_cast<unsigned char>(bytes[4]) |
                                    (static_cast<unsigned char>(bytes[5]) << 8) |
                                    (static_cast<unsigned char>(bytes[6]) << 16) |
                                    (static_cast<unsigned char>(bytes[7]) << 24);
    REQUIRE(riff_size == 4u + 8u + chunk_size + pad);
}

TEST_CASE("webp: 奇数长度 chunk 补 RIFF pad 且像素仍可回环") {
    bool saw_odd = false;
    for (std::uint32_t w = 1; w <= 8; ++w) {
        // 绿通道取 250（>=232）会用到 9-bit 码，故某些宽度会产生奇数长度像素流。
        Bitmap bm(w, 1, 4);
        for (std::uint32_t x = 0; x < w; ++x) {
            bm.set(x, 0, 0, 0);
            bm.set(x, 0, 1, 250);
            bm.set(x, 0, 2, 0);
            bm.set(x, 0, 3, 255);
        }
        const std::string bytes = encode_webp(std::vector<Bitmap>{bm});
        REQUIRE_FALSE(bytes.empty());
        const std::uint32_t chunk_size = static_cast<unsigned char>(bytes[16]) |
                                         (static_cast<unsigned char>(bytes[17]) << 8) |
                                         (static_cast<unsigned char>(bytes[18]) << 16) |
                                         (static_cast<unsigned char>(bytes[19]) << 24);
        const std::size_t pad = chunk_size & 1u;
        REQUIRE(bytes.size() == 20u + chunk_size + pad);
        const std::uint32_t riff_size = static_cast<unsigned char>(bytes[4]) |
                                        (static_cast<unsigned char>(bytes[5]) << 8) |
                                        (static_cast<unsigned char>(bytes[6]) << 16) |
                                        (static_cast<unsigned char>(bytes[7]) << 24);
        REQUIRE(riff_size == 4u + 8u + chunk_size + pad);
        Bitmap decoded;
        REQUIRE(decode_webp_to_rgba(bytes, decoded));
        REQUIRE(decoded == bm);
        saw_odd = saw_odd || (pad == 1u);
    }
    REQUIRE(saw_odd);  // 确保覆盖到奇数长度分支
}

TEST_CASE("webp: 空帧 / 超尺寸返回空串（Unsupported 契约）") {
    std::vector<Bitmap> none;
    REQUIRE(encode_webp(none).empty());
    Bitmap huge(20000, 10);
    std::vector<Bitmap> big{huge};
    REQUIRE(encode_webp(big).empty());
}

TEST_CASE("webp: 多帧重载取首帧（v1 单帧语义）") {
    Bitmap a = make_bitmap(16, 16, true);
    Bitmap b = make_bitmap(16, 16, true);
    std::vector<Bitmap> frames{a, b};
    const std::string bytes = encode_webp(frames);
    REQUIRE_FALSE(bytes.empty());
    REQUIRE(bytes.compare(0, 4, "RIFF") == 0);
}

// ---------------------------------------------------------------------------
// 回环：独立 VP8L 解码器把字节流解回像素，逐字节比对
// ---------------------------------------------------------------------------

TEST_CASE("webp: 4 通道图编码后解码像素逐字节一致") {
    for (auto [w, h] : {std::pair{64u, 48u}, {32u, 24u}, {17u, 13u}, {7u, 3u}, {1u, 1u}}) {
        const Bitmap original = make_bitmap(w, h, true);
        const std::string bytes = encode_webp(std::vector<Bitmap>{original});
        REQUIRE_FALSE(bytes.empty());
        Bitmap decoded;
        REQUIRE(decode_webp_to_rgba(bytes, decoded));
        REQUIRE(decoded == original);
    }
}

TEST_CASE("webp: 3 通道图（无 alpha）编码后解码 RGB 一致、alpha 强制 255") {
    for (auto [w, h] : {std::pair{64u, 48u}, {32u, 24u}, {17u, 13u}, {1u, 1u}}) {
        const Bitmap original = make_bitmap(w, h, false);
        const std::string bytes = encode_webp(std::vector<Bitmap>{original});
        REQUIRE_FALSE(bytes.empty());
        Bitmap decoded;
        REQUIRE(decode_webp_to_rgba(bytes, decoded));
        // 期望 RGBA：RGB 同源，A=255
        const Bitmap expected = original.with_alpha(255);
        REQUIRE(decoded == expected);
    }
}

TEST_CASE("webp: 渐变（全 256 值）解码一致") {
    Bitmap grad(48, 32, 4);
    for (std::uint32_t y = 0; y < 32; ++y) {
        for (std::uint32_t x = 0; x < 48; ++x) {
            grad.set(x, y, 0, static_cast<std::uint8_t>((x * 255) / 47));
            grad.set(x, y, 1, static_cast<std::uint8_t>((y * 255) / 31));
            grad.set(x, y, 2, 128);
            grad.set(x, y, 3, static_cast<std::uint8_t>(100 + ((x + y) & 0x1F)));
        }
    }
    const std::string bytes = encode_webp(std::vector<Bitmap>{grad});
    REQUIRE_FALSE(bytes.empty());
    Bitmap decoded;
    REQUIRE(decode_webp_to_rgba(bytes, decoded));
    REQUIRE(decoded == grad);
}

// ---------------------------------------------------------------------------
// 落盘供外部 ffmpeg / PIL 交叉校验
// ---------------------------------------------------------------------------

TEST_CASE("webp: 落盘供 ffmpeg/PIL 校验（几何图 + 渐变）") {
    Bitmap solid(64, 48, 4);
    for (std::uint32_t y = 0; y < 48; ++y) {
        for (std::uint32_t x = 0; x < 64; ++x) {
            if (x < 32) {
                solid.set(x, y, 0, 220);
                solid.set(x, y, 1, 20);
                solid.set(x, y, 2, 30);
            } else {
                solid.set(x, y, 0, 20);
                solid.set(x, y, 1, 40);
                solid.set(x, y, 2, 220);
            }
            solid.set(x, y, 3, 255);
        }
    }
    REQUIRE(write_file("tc_webp.webp", encode_webp(std::vector<Bitmap>{solid})));

    Bitmap grad(48, 32, 4);
    for (std::uint32_t y = 0; y < 32; ++y) {
        for (std::uint32_t x = 0; x < 48; ++x) {
            grad.set(x, y, 0, static_cast<std::uint8_t>((x * 255) / 47));
            grad.set(x, y, 1, static_cast<std::uint8_t>((y * 255) / 31));
            grad.set(x, y, 2, 128);
            grad.set(x, y, 3, 255);
        }
    }
    REQUIRE(write_file("tc_webp_gradient.webp", encode_webp(std::vector<Bitmap>{grad})));

    // 奇数长度 chunk（5×1、绿通道 250）：验证 RIFF pad 可被真实解码器接受。
    Bitmap odd(5, 1, 4);
    for (std::uint32_t x = 0; x < 5; ++x) {
        odd.set(x, 0, 0, 0);
        odd.set(x, 0, 1, 250);
        odd.set(x, 0, 2, 0);
        odd.set(x, 0, 3, 255);
    }
    REQUIRE(write_file("tc_webp_odd.webp", encode_webp(std::vector<Bitmap>{odd})));
}
