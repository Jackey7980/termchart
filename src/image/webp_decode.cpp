// webp_decode.cpp —— 原生零依赖 VP8L lossless WebP 解码
//
// 与 encode_webp 对称：encode 写出的位流这里原样读回。解码器从码长独立复算
// canonical 码（按位反转发射、按反转键查表），不信任编码器的码字分配；
// 这正是 test_webp_encode 中“独立最小解码器”提升为正式 API 的版本，其回环
// 正确性已由该单测覆盖。
#include "termchart/image/webp_decode.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace termchart::image {
namespace {

// 码长码位置顺序（VP8L 规范，与编码器、解码器共享）。
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
std::vector<std::pair<std::uint32_t, int>> canonical_codes(
    const std::vector<int>& lengths) {
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
        h.lut[(static_cast<std::uint32_t>(lengths[s]) << 16) | rev] =
            static_cast<int>(s);
    }
    return h;
}

struct BitReader {
    const std::uint8_t* d;
    std::size_t size;
    std::size_t byte_pos;
    int bit_pos;

    BitReader(const std::uint8_t* data, std::size_t n, std::size_t start = 0)
        : d(data), size(n), byte_pos(start), bit_pos(0) {}

    int read_bit() {
        if (byte_pos >= size) {
            return 0;  // 越界视作 0（坏流）
        }
        const int b = (d[byte_pos] >> bit_pos) & 1;
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

bool decode_symbol(const Huffman& h, BitReader& br, int& out_sym) {
    std::uint32_t window = 0;
    int len = 0;
    for (;;) {
        window |= static_cast<std::uint32_t>(br.read_bit()) << len;
        ++len;
        auto it = h.lut.find((static_cast<std::uint32_t>(len) << 16) | window);
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
    const Huffman clcc = build_huffman(std::vector<int>(clcc_len, clcc_len + 19));
    out_lengths.assign(static_cast<std::size_t>(alphabet_size), 0);
    for (int i = 0; i < alphabet_size; ++i) {
        int sym;
        if (!decode_symbol(clcc, br, sym)) {
            return false;
        }
        // 码长码符号的取值即码长（位置 == 取值；本编码器只用 0/1/8/9）。
        if (sym != 0 && sym != 1 && sym != 8 && sym != 9) {
            return false;
        }
        out_lengths[static_cast<std::size_t>(i)] = sym;
    }
    return true;
}

}  // namespace

Result<Bitmap> decode_webp(const std::uint8_t* data, std::size_t size) noexcept {
    // —— RIFF / WEBP / VP8L 容器 ——
    if (data == nullptr || size < 20) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }
    if (data[0] != 'R' || data[1] != 'I' || data[2] != 'F' || data[3] != 'F') {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }
    if (data[8] != 'W' || data[9] != 'E' || data[10] != 'B' || data[11] != 'P') {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }
    if (data[12] != 'V' || data[13] != 'P' || data[14] != '8' || data[15] != 'L') {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }
    const std::uint32_t chunk_size =
        static_cast<std::uint32_t>(data[16]) |
        (static_cast<std::uint32_t>(data[17]) << 8) |
        (static_cast<std::uint32_t>(data[18]) << 16) |
        (static_cast<std::uint32_t>(data[19]) << 24);
    const std::size_t pad = chunk_size & 1u;  // 奇数 chunk 的 RIFF 补齐字节
    if (20u + static_cast<std::size_t>(chunk_size) + pad > size) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }

    BitReader br(data + 20, static_cast<std::size_t>(chunk_size), 0);

    // —— VP8L 头 ——
    if (br.read_bits(8) != 0x2Fu) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }
    const std::uint32_t w = br.read_bits(14) + 1;
    const std::uint32_t h = br.read_bits(14) + 1;
    if (w == 0 || h == 0 || w > 16383 || h > 16383) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }
    const bool has_alpha = br.read_bits(1) != 0;
    (void)br.read_bits(3);  // version

    // —— 仅支持无损字面量流（无变换 / 色彩缓存 / 元霍夫曼）——
    if (br.read_bits(1) != 0) {
        return Unexpected<ErrorCode>(ErrorCode::Unsupported);
    }
    if (br.read_bits(1) != 0) {
        return Unexpected<ErrorCode>(ErrorCode::Unsupported);
    }
    if (br.read_bits(1) != 0) {
        return Unexpected<ErrorCode>(ErrorCode::Unsupported);
    }

    // —— 5 棵树：GREEN(280) / RED(256) / BLUE(256) / ALPHA(256) / DIST(40) ——
    constexpr int kAlphabet[5] = {280, 256, 256, 256, 40};
    Huffman trees[5];
    std::vector<int> lengths;
    for (int t = 0; t < 5; ++t) {
        if (!read_tree(br, kAlphabet[t], lengths)) {
            return Unexpected<ErrorCode>(ErrorCode::Unsupported);
        }
        trees[t] = build_huffman(lengths);
    }

    // —— 逐像素解码（G,R,B,A 四符号）——
    Bitmap out(w, h, 4);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            int g = 0, r = 0, b = 0, a = 0;
            if (!decode_symbol(trees[0], br, g)) {
                return Unexpected<ErrorCode>(ErrorCode::ParseError);
            }
            if (!decode_symbol(trees[1], br, r)) {
                return Unexpected<ErrorCode>(ErrorCode::ParseError);
            }
            if (!decode_symbol(trees[2], br, b)) {
                return Unexpected<ErrorCode>(ErrorCode::ParseError);
            }
            if (!decode_symbol(trees[3], br, a)) {
                return Unexpected<ErrorCode>(ErrorCode::ParseError);
            }
            if (!has_alpha) {
                a = 255;  // 无 alpha 时强制不透明
            }
            out.set(x, y, 0, static_cast<std::uint8_t>(r));
            out.set(x, y, 1, static_cast<std::uint8_t>(g));
            out.set(x, y, 2, static_cast<std::uint8_t>(b));
            out.set(x, y, 3, static_cast<std::uint8_t>(a));
        }
    }
    return out;
}

}  // namespace termchart::image
