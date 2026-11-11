// quantize.cpp —— HEVC 量化 / 逆量化（4/8/16/32 + transform_skip）
//
// 逆量化逐常量对齐 ffmpeg libavcodec/hevc/cabac.c + dsp_template.c：
//   deq = (level * scale * scale_m + add) >> shift
// 其中 scale = level_scale[qp%6] << (qp/6)、scale_m = 16（无缩放矩阵）。
//
// 非 skip：shift = bit_depth + log2_trafo_size - 5（8bit 4/8/16/32 → 5/6/7/8），
//   其后接 idct（两级移位 7/12 与矩阵增益 2^(log2+12) 恰好补足余量）。
// skip（transform_skip_flag=1）：ffmpeg 走【两段】逆量化，不能合并成一次移位：
//   段1（cabac.c:1556，与非 skip 共用）t = clip16((level*scale*16 + 1<<(s1-1)) >> s1)
//                                       s1 = bit_depth + log2_trafo_size - 5 = 3+log2
//   段2（dsp_template.c:127 dequant）  r = (t + 1<<(s2-1)) >> s2
//                                       s2 = 15 - bit_depth - log2_trafo_size = 7-log2
//   s1+s2 = 10 与 log2_size 无关，净步长与非 skip 路径一致（HEVC 设计如此）。
// 旧实现把 skip 写死成 shift = bit_depth - 5 = 3，净移位比 ffmpeg 少 7 位（差 128 倍），
// 且自往返两侧用同一份错误 → 单测全绿、外部 ffmpeg 解码全错。此处按源码逐段复刻。
// 前向为各段舍入的合并逆：level = round(coeff * 2^10 / (scale * scale_m))。
#include "quantize.hpp"

#include <algorithm>
#include <cmath>

namespace termchart::image::codec::hevc {
namespace {

constexpr std::uint8_t kLevelScale[6] = {40, 45, 51, 57, 64, 72};
constexpr int kBitDepth = 8;
constexpr int kScaleM = 16;  // LevelScale（默认缩放表全 16，transform_skip 同）

// ffmpeg 两段式 skip 逆量化的分段移位（8bit 下 s2 恒 > 0）。
int skip_shift1(int log2_size) { return kBitDepth + log2_size - 5; }   // 3+log2
int skip_shift2(int log2_size) { return 15 - kBitDepth - log2_size; }  // 7-log2

int shift_for(int log2_size, bool skip) {
    if (skip) return skip_shift1(log2_size) + skip_shift2(log2_size);  // 8bit → 10
    return kBitDepth + log2_size - 5;                                   // 5/6/7/8
}

}  // namespace

int quant(const std::int32_t* coeff, std::int16_t* level, int qp, int log2_size,
          bool transform_skip) {
    qp = std::clamp(qp, 0, 51);
    const int rem = qp % 6;
    const int div = qp / 6;
    const int shift = shift_for(log2_size, transform_skip);
    // 逆量化：deq(level) = level * scale * scale_m / 2^shift
    //   ⇒ level = round(coeff * 2^shift / (scale * scale_m))
    const std::int64_t den = (static_cast<std::int64_t>(kLevelScale[rem]) << div) * kScaleM;
    const std::int64_t add = 1LL << (shift - 1);
    const int n = 1 << (2 * log2_size);
    int nonzero = 0;
    for (int k = 0; k < n; ++k) {
        const std::int64_t a = std::llabs(static_cast<std::int64_t>(coeff[k]));
        const std::int64_t v = (a * (1LL << shift) + add) / den;
        const std::int64_t m = std::min<std::int64_t>(v, 32767);
        level[k] = static_cast<std::int16_t>(coeff[k] < 0 ? -m : m);
        nonzero += (v != 0) ? 1 : 0;
    }
    return nonzero;
}

void dequant(const std::int16_t* level, std::int32_t* coeff, int qp, int log2_size,
             bool transform_skip) {
    qp = std::clamp(qp, 0, 51);
    const int rem = qp % 6;
    const int div = qp / 6;
    const std::int64_t scale = static_cast<std::int64_t>(kLevelScale[rem]) << div;
    const int n = 1 << (2 * log2_size);
    if (transform_skip) {
        // 逐段复刻 ffmpeg：段1 在 cabac.c 里 clip16，段2 在 dsp_template.c 的
        // dequant 里再 clip16。合并成一次 >>10 会因中间舍入/截断不同而与解码端错开。
        const int s1 = skip_shift1(log2_size);
        const int s2 = skip_shift2(log2_size);
        const std::int64_t add1 = 1LL << (s1 - 1);
        const std::int64_t add2 = 1LL << (s2 - 1);
        for (int k = 0; k < n; ++k) {
            const std::int64_t t = (static_cast<std::int64_t>(level[k]) * scale * kScaleM
                                    + add1) >> s1;
            const std::int64_t tc = std::clamp<std::int64_t>(t, -32768, 32767);
            const std::int64_t r = (tc + add2) >> s2;
            coeff[k] = static_cast<std::int32_t>(std::clamp<std::int64_t>(r, -32768, 32767));
        }
        return;
    }
    const int shift = shift_for(log2_size, false);
    const std::int64_t add = 1LL << (shift - 1);
    for (int k = 0; k < n; ++k) {
        const std::int64_t dq =
            (static_cast<std::int64_t>(level[k]) * scale * kScaleM + add) >> shift;
        coeff[k] = static_cast<std::int32_t>(std::clamp<std::int64_t>(dq, -32768, 32767));
    }
}

int quant_4x4(const std::int32_t* coeff, std::int16_t* level, int qp) {
    return quant(coeff, level, qp, 2, false);
}

void dequant_4x4(const std::int16_t* level, std::int32_t* coeff, int qp) {
    dequant(level, coeff, qp, 2, false);
}

double quant_step(int qp) {
    qp = std::clamp(qp, 0, 51);
    const int rem = qp % 6;
    const int div = qp / 6;
    return std::ldexp(static_cast<double>(kLevelScale[rem]), div) / 2.0;
}

}  // namespace termchart::image::codec::hevc
