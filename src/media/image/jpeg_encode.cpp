// jpeg_encode.cpp —— 自包含 baseline JPEG 编码器（顺序、哈夫曼、YCbCr 4:4:4）
//
// 流程：RGB → YCbCr → 每个 8×8 块做等级偏移(−128) → 浮点 FDCT → 量化 → 之字形排列
// → DC 差分 + 哈夫曼、AC 游程 + 哈夫曼 → 熵编码为扫描数据；外层封装 JFIF 标记
// （SOI/APP0/DQT/SOF0/DHT/SOS/EOI）。量化表按 quality 缩放；哈夫曼表用 JPEG 规范
// Annex K 的标准表（亮度/色度各 DC/AC 一套）。扫描数据做 0xFF 字节填充（FF→FF 00）。
// 不依赖任何外部库——与 png_encode 同哲学。
#include "termchart/media/image/jpeg_encode.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace termchart::image {
namespace {

// ── 之字形扫描：自然序下标 → 之字序位置（即标准 zigzag 表的逆）──────────────
// 标准 JPEG zigzag 给出的是 scan→natural：{0,1,8,16,9,2,...}。
// 这里需要 natural→scan，因此取它的逆。两处使用（写 DQT、Fdct 后排 AC）
// 都以自然序下标 i 为索引，把系数放到 kZigZag[i] 这个之字序位置。
constexpr int kZigZag[64] = {
    0,  1,  5,  6, 14, 15, 27, 28,  2,  4,  7, 13, 16, 26, 29, 42,
    3,  8, 12, 17, 25, 30, 41, 43,  9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54, 20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61, 35, 36, 48, 49, 57, 58, 62, 63};

// ── 标准量化表（自然序）────────────────────────────────────────────────────
constexpr std::uint8_t kQuantLum[64] = {
    16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99};

constexpr std::uint8_t kQuantChrom[64] = {
    17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99, 47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99};

// ── 标准哈夫曼表（JPEG 规范 Annex K）：BITS[16] + HUFFVAL[] ──────────────────
constexpr int kDcLumBits[16]  = {0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};
constexpr int kDcLumVals[12]  = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

constexpr int kAcLumBits[16]  = {0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125};
constexpr int kAcLumVals[162] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
    0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
    0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA};

constexpr int kDcChromBits[16] = {0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
constexpr int kDcChromVals[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

constexpr int kAcChromBits[16] = {0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119};
constexpr int kAcChromVals[162] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0,
    0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26,
    0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5,
    0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3,
    0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
    0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA};

// 哈夫曼码表：code[symbol] + len[symbol]。
struct Huff {
    std::uint16_t code[256];
    std::uint8_t  len[256];
};

// 由 BITS/HUFFVAL 构造规范哈夫曼码（symbol 即 HUFFVAL 的顺序，code 按长度升序分配）。
void build_huff(Huff& h, const int* bits, const int* vals, int nvals) {
    // 先全部清零：symbol 值是任意字节（如 AC 的 0xF0/0xA2..0xFA），
    // 不能按条目数 nvals 当索引清零，否则会误清 >=nvals 的合法高符号。
    for (int i = 0; i < 256; ++i) { h.code[i] = 0; h.len[i] = 0; }
    int code = 0, k = 0;
    for (int l = 1; l <= 16; ++l) {
        for (int i = 0; i < bits[l - 1]; ++i) {
            const int sym = vals[k++];
            h.code[sym] = static_cast<std::uint16_t>(code);
            h.len[sym]  = static_cast<std::uint8_t>(l);
            ++code;
        }
        code <<= 1;
    }
    (void)nvals;
}

// ── 位写入器（MSB 优先，扫描数据做 0xFF 填充）───────────────────────────────
struct BitWriter {
    std::string out;
    int  cur = 0;     // 当前累积字节
    int  nbits = 0;   // 已累积位数

    void write(int value, int len) {
        for (int i = len - 1; i >= 0; --i) {
            const int bit = (value >> i) & 1;
            cur = (cur << 1) | bit;
            if (++nbits == 8) {
                flush_byte();
            }
        }
    }
    void flush_byte() {
        out.push_back(static_cast<char>(static_cast<std::uint8_t>(cur)));
        if (cur == 0xFF) out.push_back('\0');  // 字节填充
        cur = 0; nbits = 0;
    }
    void flush() {
        if (nbits > 0) { cur <<= (8 - nbits); flush_byte(); }
    }
};

int category(int val) {  // val != 0 → 1..11
    int a = std::abs(val), c = 0;
    while (a) { a >>= 1; ++c; }
    return c;
}

// 写幅度位（正数直接写，负数写 val + (1<<c) - 1）。
void write_amplitude(BitWriter& bw, int val, int cat) {
    if (val >= 0) {
        bw.write(val, cat);
    } else {
        bw.write(val + (1 << cat) - 1, cat);
    }
}

// 浮点 FDCT（8×8，可分离两重 1D DCT-II）。c[u][x] 只放 cos 核，归一化因子
// C(u)=（u==0?1/√2:1）与 2/N=0.25 拆成每维 C(u)*0.5，避免 DC 项被重复归一化。
void fdct_block(const double in[64], double out[64]) {
    static double c[8][8];
    static bool init = false;
    if (!init) {
        for (int u = 0; u < 8; ++u)
            for (int x = 0; x < 8; ++x)
                c[u][x] = std::cos((2 * x + 1) * u * 3.14159265358979323846 / 16.0);
        init = true;
    }
    double tmp[64];
    for (int y = 0; y < 8; ++y)
        for (int u = 0; u < 8; ++u) {
            double s = 0;
            for (int x = 0; x < 8; ++x) s += in[y * 8 + x] * c[u][x];
            const double cu = (u == 0) ? 0.7071067811865476 : 1.0;
            tmp[y * 8 + u] = s * 0.5 * cu;
        }
    for (int u = 0; u < 8; ++u)
        for (int v = 0; v < 8; ++v) {
            double s = 0;
            for (int y = 0; y < 8; ++y) s += tmp[y * 8 + u] * c[v][y];
            const double cv = (v == 0) ? 0.7071067811865476 : 1.0;
            out[v * 8 + u] = s * 0.5 * cv;  // JPEG 自然序 = [垂直频率][水平频率]
        }
}

// 量化表缓存（natural 序用于量化，zigzag 序用于写入 DQT）。
struct QuantTables {
    double lum_n[64];     // 量化除数（natural）
    double chrom_n[64];
    std::uint8_t lum_z[64];   // 写入 DQT 的字节（zigzag）
    std::uint8_t chrom_z[64];
};

void build_quant(QuantTables& q, int quality) {
    int qual = quality < 1 ? 1 : (quality > 100 ? 100 : quality);
    int scale = (qual < 50) ? (5000 / qual) : (200 - 2 * qual);
    auto mk = [&](const std::uint8_t* src, double* nat, std::uint8_t* zz) {
        for (int i = 0; i < 64; ++i) {
            int v = (src[i] * scale + 50) / 100;
            if (v < 1) v = 1;
            if (v > 255) v = 255;
            nat[i] = static_cast<double>(v);
            zz[kZigZag[i]] = static_cast<std::uint8_t>(v);
        }
    };
    mk(kQuantLum, q.lum_n, q.lum_z);
    mk(kQuantChrom, q.chrom_n, q.chrom_z);
}

}  // namespace

std::string encode_jpeg(const Bitmap& bitmap, int quality) {
    if (bitmap.empty() || bitmap.width() == 0 || bitmap.height() == 0) {
        return std::string{};
    }
    const int W = static_cast<int>(bitmap.width());
    const int H = static_cast<int>(bitmap.height());
    const int Wp = ((W + 7) / 8) * 8;
    const int Hp = ((H + 7) / 8) * 8;

    // RGB → YCbCr 平面（BT.601），补边用边缘像素。
    const size_t npix = static_cast<size_t>(Wp) * static_cast<size_t>(Hp);
    std::vector<double> Y(npix), Cb(npix), Cr(npix);
    
    for (int y = 0; y < Hp; ++y) {
        const int sy = y < H ? y : H - 1;
        for (int x = 0; x < Wp; ++x) {
            const int sx = x < W ? x : W - 1;
            const double r = bitmap.at(static_cast<std::uint32_t>(sx),
                                        static_cast<std::uint32_t>(sy), 0);
            const double g = bitmap.at(static_cast<std::uint32_t>(sx),
                                        static_cast<std::uint32_t>(sy), 1);
            const double b = bitmap.at(static_cast<std::uint32_t>(sx),
                                        static_cast<std::uint32_t>(sy), 2);
            const int idx = y * Wp + x;
            Y[static_cast<size_t>(idx)]  =  0.299 * r + 0.587 * g + 0.114 * b;
            Cb[static_cast<size_t>(idx)] = -0.168736 * r - 0.331264 * g + 0.5 * b + 128.0;
            Cr[static_cast<size_t>(idx)] =  0.5 * r - 0.418688 * g - 0.081312 * b + 128.0;
        }
    }

    QuantTables qt;
    build_quant(qt, quality);

    // 哈夫曼表
    Huff dc_lum{}, ac_lum{}, dc_chrom{}, ac_chrom{};
    build_huff(dc_lum, kDcLumBits, kDcLumVals, 12);
    build_huff(ac_lum, kAcLumBits, kAcLumVals, 162);
    build_huff(dc_chrom, kDcChromBits, kDcChromVals, 12);
    build_huff(ac_chrom, kAcChromBits, kAcChromVals, 162);

    std::string out;
    auto emit_byte = [&](int b) { out.push_back(static_cast<char>(static_cast<std::uint8_t>(b))); };
    auto emit_word = [&](int v) { emit_byte((v >> 8) & 0xFF); emit_byte(v & 0xFF); };
    auto emit_marker = [&](int m) { emit_byte(0xFF); emit_byte(m); };

    // SOI
    emit_marker(0xD8);
    // APP0 JFIF
    emit_marker(0xE0); emit_word(16);
    out += "JFIF"; emit_byte(0); emit_byte(1); emit_byte(1);
    emit_byte(0); emit_word(1); emit_word(1); emit_byte(0); emit_byte(0);
    // DQT（亮度 id 0、色度 id 1，均 8-bit 精度）
    auto emit_dqt = [&](const std::uint8_t* tbl, int id) {
        emit_marker(0xDB); emit_word(64 + 1 + 2); emit_byte(id);  // PQ=0,Tq=id
        for (int i = 0; i < 64; ++i) emit_byte(tbl[i]);
    };
    emit_dqt(qt.lum_z, 0);
    emit_dqt(qt.chrom_z, 1);
    // SOF0（Lp 含 2 字节长度域本身：payload 15 + 2 = 17）
    emit_marker(0xC0); emit_word(2 + 1 + 2 + 1 + 3 * 3 + 2);
    emit_byte(8); emit_word(H); emit_word(W); emit_byte(3);
    emit_byte(1); emit_byte(0x11); emit_byte(0);  // Y: 4:4:4, qtbl 0
    emit_byte(2); emit_byte(0x11); emit_byte(1);  // Cb
    emit_byte(3); emit_byte(0x11); emit_byte(1);  // Cr
    // DHT（4 张表）
    auto emit_dht = [&](const int* bits, const int* vals, int nvals, int cls_id) {
        int total = 0; for (int i = 0; i < 16; ++i) total += bits[i];
        emit_marker(0xC4); emit_word(2 + 1 + 16 + total);
        emit_byte(cls_id);
        for (int i = 0; i < 16; ++i) emit_byte(bits[i]);
        for (int i = 0; i < total; ++i) emit_byte(vals[i]);
    };
    emit_dht(kDcLumBits, kDcLumVals, 12, 0x00);
    emit_dht(kAcLumBits, kAcLumVals, 162, 0x10);
    emit_dht(kDcChromBits, kDcChromVals, 12, 0x01);
    emit_dht(kAcChromBits, kAcChromVals, 162, 0x11);
    // SOS
    emit_marker(0xDA); emit_word(2 + 1 + 2 * 3 + 3);
    emit_byte(3);
    emit_byte(1); emit_byte(0x00);
    emit_byte(2); emit_byte(0x11);
    emit_byte(3); emit_byte(0x11);
    emit_byte(0); emit_byte(0x3F); emit_byte(0);  // Ss, Se, Ah/Al

    // 熵编码（4:4:4，每个 MCU = 一块 Y + 一块 Cb + 一块 Cr）
    BitWriter bw;
    double block[64], dct[64], qz[64];
    int prev_dc[3] = {0, 0, 0};
    const double* planes[3] = {Y.data(), Cb.data(), Cr.data()};
    const Huff* dc_tbl[3] = {&dc_lum, &dc_chrom, &dc_chrom};
    const Huff* ac_tbl[3] = {&ac_lum, &ac_chrom, &ac_chrom};
    const double* qnat[3] = {qt.lum_n, qt.chrom_n, qt.chrom_n};

    for (int by = 0; by < Hp; by += 8) {
        for (int bx = 0; bx < Wp; bx += 8) {
            for (int comp = 0; comp < 3; ++comp) {
                const double* plane = planes[comp];
                // 取 8×8 块
                for (int y = 0; y < 8; ++y)
                    for (int x = 0; x < 8; ++x)
                        block[y * 8 + x] = plane[(by + y) * Wp + (bx + x)] - 128.0;
                fdct_block(block, dct);
                // 量化 + 之字序
                for (int i = 0; i < 64; ++i) {
                    double qv = std::round(dct[i] / qnat[comp][i]);
                    if (qv > 1023) qv = 1023;
                    if (qv < -1024) qv = -1024;
                    qz[kZigZag[i]] = qv;
                }
                // DC
                const Huff& dct_h = *dc_tbl[comp];
                const Huff& ac_h = *ac_tbl[comp];
                int dc = static_cast<int>(qz[0]);
                int diff = dc - prev_dc[comp];
                prev_dc[comp] = dc;
                if (diff == 0) {
                    bw.write(dct_h.code[0], dct_h.len[0]);
                } else {
                    int cat = category(diff);
                    bw.write(dct_h.code[cat], dct_h.len[cat]);
                    write_amplitude(bw, diff, cat);
                }
                // AC：游程编码
                int run = 0;
                for (int k = 1; k < 64; ++k) {
                    int v = static_cast<int>(qz[k]);
                    if (v == 0) {
                        ++run;
                        if (run == 16) {
                            bw.write(ac_h.code[0xF0], ac_h.len[0xF0]);  // ZRL
                            run = 0;
                        }
                    } else {
                        while (run >= 16) {
                            bw.write(ac_h.code[0xF0], ac_h.len[0xF0]);
                            run -= 16;
                        }
                        int size = category(v);
                        int sym = (run << 4) | size;
                        bw.write(ac_h.code[sym], ac_h.len[sym]);
                        write_amplitude(bw, v, size);
                        run = 0;
                    }
                }
                if (run > 0) {
                    bw.write(ac_h.code[0x00], ac_h.len[0x00]);  // EOB
                }
            }
        }
    }
    bw.flush();
    out += bw.out;

    // EOI
    emit_marker(0xD9);
    return out;
}

}  // namespace termchart::image
