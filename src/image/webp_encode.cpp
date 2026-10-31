// webp_encode.cpp —— 自包含 VP8L lossless WebP 编码器（零依赖）
//
// 比特流严格按 WebP Lossless Bitstream 规范（RIFF/WEBP/VP8L 容器 + VP8L 熵编码）：
//   · 整个 VP8L 流 LSB-first 位序（BitWriter 先写 bit0）。
//   · 36/40-bit 头部：签名(8) / width-1(14) / height-1(14) / has_alpha(1) /
//     version(3) / 无变换(1) / 无色彩缓存(1)。
//   · 5 棵霍夫曼树：GREEN(字母表 280) / RED(256) / BLUE(256) / ALPHA(256) /
//     DIST(40)，均用非 simple 路径，码长经码长码（code-length-code）发射。
//   · 每像素按 **G,R,B,A** 顺序写 4 个字面量符号；即使 has_alpha=0 也写满 4 个
//     （alpha 字面值恒为 255，解码器据头部 alpha 位强制输出 255）。
//
// 码字发射约定（与 libwebp 解码器严格对齐）：解码器把读到的 LSB-first 位序列 reverse
// 后查表，因此码值必须以"按位反转"形式写入流中（reverse_bits）。
//
// v1 故意采用定长字面量（无 LZ77 / 无色彩缓存 / 无变换），这是最不易出错、且
// libwebp/ffmpeg/PIL 必可解的形态；压缩优化在后续任务叠加，不阻塞 v1 正确性。
#include "termchart/image/webp_encode.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace termchart::image {
namespace {

// LSB-first 位写入器（先写 bit0）。
struct BitWriter {
    std::string out;
    int nbits = 0;

    void put_bit(int b) {
        if (nbits == 0) {
            out.push_back(static_cast<char>(0));
        }
        if (b & 1) {
            out.back() |= static_cast<char>(1u << nbits);
        }
        nbits = (nbits + 1) & 7;
    }

    void put_bits(std::uint32_t v, int n) {
        for (int i = 0; i < n; ++i) {
            put_bit(static_cast<int>((v >> i) & 1u));
        }
    }
};

// 把低 ln 位按位反转（VP8L 码字在流中以反转序发射）。
inline std::uint32_t reverse_bits(std::uint32_t c, int ln) {
    std::uint32_t r = 0;
    for (int i = 0; i < ln; ++i) {
        r = (r << 1) | ((c >> i) & 1u);
    }
    return r;
}

// 标准 canonical 霍夫曼码分配：每符号码长 -> 每符号 (码值, 长度)。
// length==0 的符号码值记为 0。此分配是确定性过程，解码端可独立复算。
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

// 码长码（code-length-code）位置顺序（规范固定）。
constexpr int kCodeLengthCodeOrder[19] = {17, 18, 0, 1, 2, 3, 4, 5, 16, 6,
                                          7, 8, 9, 10, 11, 12, 13, 14, 15};

// 码长码本身：符号 0/1/8/9 各长度 2（其余 0）。Kraft = 4/4 = 1。
constexpr int kClccPositions[4] = {0, 1, 8, 9};

// 像素码长（字面量值）-> 码长码符号：码长码符号的取值等于其位置。
inline int clcc_symbol_for_length(int len) {
    switch (len) {
        case 0: return 0;
        case 1: return 1;
        case 8: return 8;
        case 9: return 9;
        default: return 0;  // 本编码器只产生 0/1/8/9 四种码长
    }
}

// 发射一棵树（非 simple 路径）：元头 + 各符号码长（经码长码发射）。
void emit_tree(BitWriter& bw,
               const std::vector<std::pair<std::uint32_t, int>>& clcc_codes,
               const std::vector<int>& codelens) {
    bw.put_bit(0);  // simple_code = 0

    // 元头：4-bit 存 (num_codes - 4)，覆盖码长码位置 {0,1,8,9}
    //（落在 order 索引 2/3/11/12）。
    constexpr int num_codes = 13;
    bw.put_bits(static_cast<std::uint32_t>(num_codes - 4), 4);
    int clcc_len[19] = {0};
    for (int p : kClccPositions) {
        clcc_len[p] = 2;
    }
    for (int i = 0; i < num_codes; ++i) {
        const int idx = kCodeLengthCodeOrder[i];
        bw.put_bits(static_cast<std::uint32_t>(clcc_len[idx]), 3);
    }

    bw.put_bit(0);  // use_length = 0

    const auto codes = canonical_codes(codelens);
    for (int L : codelens) {
        const int sym = clcc_symbol_for_length(L);
        const auto& [c, ln] = clcc_codes[static_cast<std::size_t>(sym)];
        bw.put_bits(reverse_bits(c, ln), ln);
    }
}

inline void append_le32(std::string& s, std::uint32_t v) {
    s.push_back(static_cast<char>(v & 0xFF));
    s.push_back(static_cast<char>((v >> 8) & 0xFF));
    s.push_back(static_cast<char>((v >> 16) & 0xFF));
    s.push_back(static_cast<char>((v >> 24) & 0xFF));
}

}  // namespace

std::string encode_webp(std::span<const Bitmap> frames, const EncodeOptions&) {
    if (frames.empty()) {
        return {};
    }
    const Bitmap& img = frames[0];
    const std::uint32_t w = img.width();
    const std::uint32_t h = img.height();
    if (w == 0 || h == 0 || w > 16383 || h > 16383) {
        return {};  // 14-bit 尺寸字段上限
    }
    const bool has_alpha = (img.channels() == 4);

    // 码长码 canonical（4 个符号各 2-bit），所有树共用。
    int clcc_len_arr[19] = {0};
    for (int p : kClccPositions) {
        clcc_len_arr[p] = 2;
    }
    const auto clcc_codes =
        canonical_codes(std::vector<int>(clcc_len_arr, clcc_len_arr + 19));

    // 5 棵树的码长：GREEN 字母表 280（232 个 8-bit + 48 个 9-bit，Kraft=1）；
    // R/B/A 字母表 256 全 8-bit；DIST 字母表 40，仅符号 0/1 各 1-bit。
    std::vector<int> green(280);
    for (int i = 0; i < 280; ++i) {
        green[static_cast<std::size_t>(i)] = (i < 232) ? 8 : 9;
    }
    std::vector<int> flat256(256, 8);
    std::vector<int> dist(40, 0);
    dist[0] = 1;
    dist[1] = 1;

    BitWriter bw;

    // —— VP8L 头部（签名 + 32 bit 字段）——
    bw.put_bits(0x2Fu, 8);       // 签名
    bw.put_bits(w - 1, 14);      // width-1
    bw.put_bits(h - 1, 14);      // height-1
    bw.put_bits(has_alpha ? 1u : 0u, 1);
    bw.put_bits(0, 3);           // version

    // —— 变换 / 色彩缓存 / 元霍夫曼：均无（各 1 bit = 0）——
    // 注意：color cache 位之后还有一位 meta-Huffman 标志（0 = 单一 htree group）。
    // 漏发这一位会让整条熵流错位 1 bit，真实解码器（libwebp / ffmpeg）直接判 BITSTREAM_ERROR。
    bw.put_bits(0, 1);
    bw.put_bits(0, 1);
    bw.put_bits(0, 1);

    // —— 5 棵霍夫曼树：GREEN, RED, BLUE, ALPHA, DIST ——
    emit_tree(bw, clcc_codes, green);
    emit_tree(bw, clcc_codes, flat256);
    emit_tree(bw, clcc_codes, flat256);
    emit_tree(bw, clcc_codes, flat256);
    emit_tree(bw, clcc_codes, dist);

    // —— 像素：逐像素 G,R,B,A 字面量符号 ——
    const auto green_codes = canonical_codes(green);
    const auto red_codes = canonical_codes(flat256);
    const auto blue_codes = canonical_codes(flat256);
    const auto alpha_codes = canonical_codes(flat256);
    auto emit_symbol = [&](const std::vector<std::pair<std::uint32_t, int>>& codes,
                            std::uint8_t val) {
        const auto& [c, ln] = codes[val];
        bw.put_bits(reverse_bits(c, ln), ln);
    };
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            // Bitmap 通道布局 R=0, G=1, B=2, A=3；VP8L 流顺序为 G,R,B,A。
            const std::uint8_t g = img.at(x, y, 1);
            const std::uint8_t r = img.at(x, y, 0);
            const std::uint8_t b = img.at(x, y, 2);
            const std::uint8_t a =
                has_alpha ? img.at(x, y, 3) : static_cast<std::uint8_t>(255);
            emit_symbol(green_codes, g);
            emit_symbol(red_codes, r);
            emit_symbol(blue_codes, b);
            emit_symbol(alpha_codes, a);
        }
    }

    // —— 组装 RIFF/WEBP/VP8L 容器 ——
    // RIFF 规定 chunk 数据长度为奇数时须补一个 pad 字节：不计入 chunk_size，
    // 但计入 RIFF 总长度。缺了它会令严格解析器（如 ffmpeg）报 image data not found。
    const std::string vp8l = bw.out;
    const std::uint32_t vp8l_size = static_cast<std::uint32_t>(vp8l.size());
    const std::uint32_t pad = vp8l_size & 1u;
    std::string file;
    file.reserve(20 + vp8l_size + pad);
    file += "RIFF";
    append_le32(file, static_cast<std::uint32_t>(4 + 8 + vp8l_size + pad));
    file += "WEBP";
    file += "VP8L";
    append_le32(file, vp8l_size);
    file += vp8l;
    if (pad != 0) {
        file.push_back('\0');
    }
    return file;
}

}  // namespace termchart::image
