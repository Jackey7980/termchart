// transform.cpp —— HEVC 4x4 整数变换（DCT-II / DST-VII）
//
// inv_* 逐常量照搬 ffmpeg libavcodec/hevc/dsp_template.c 的 idct_4x4 与
// transform_4x4_luma（TR_4 / TR_4x4_LUMA + SCALE(clip_int16)），先列(shift=7)后行(shift=12)。
// 这样编码端重建像素 == ffmpeg 解码像素（两者都走 inv_*）。
//
// fwd_* 为对应正向变换：DCT-II 用 x265 g_t4 可分矩阵（T·r·T，两段 >>1/>>8，T²=2^14·I，与
// ffmpeg idct 精确互逆）；DST-VII 用 x265 fastForwardDst 蝶形（两段 >>1/>>8），与 ffmpeg
// transform_4x4_luma 构成整数互逆对。中间值走 int32，不提前截断。
#include "transform.hpp"

#include <algorithm>
#include <cstdint>

namespace termchart::image::codec::hevc {
namespace {

// 8-bit 下的两次 SCALE 移位（与 ffmpeg BIT_DEPTH=8 一致）。
constexpr int kInvShift1 = 7;    // add = 1<<6
constexpr int kInvShift2 = 12;   // add = 1<<11

inline std::int16_t clip_int16(std::int32_t x) {
    return static_cast<std::int16_t>(std::clamp(x, -32768, 32767));
}
inline std::int32_t clip_int32(std::int32_t x) {
    return std::clamp(x, -32768, 32767);
}

// ── DCT-II 4x4 正向核（x265 g_t4，按 64 缩放），与 ffmpeg idct 互逆 ──────────
constexpr std::int16_t kDct[4][4] = {
    {64,  64,  64,  64},
    {83,  36, -36, -83},
    {64, -64, -64,  64},
    {36, -83,  83, -36},
};

// ── DST-VII 4x4 正向核（HEVC 规范表 8-3，按 128 缩放），逆为 Mᵀ ──────────────
constexpr std::int16_t kDst[4][4] = {
    {29,  55,  74,  84},
    {74,  74,   0, -74},
    {84, -29, -74,  55},
    {55, -84,  74, -29},
};

// ffmpeg TR_4（DCT-II 逆），assign = SCALE（av_clip_int16）。
// 精确复刻 ffmpeg idct_4x4：先「列」(step=4, shift=7) 后「行」(step=1, shift=12)。
void inv_dct1_cols(std::int32_t* c) {  // 列变换（step=4）
    for (int i = 0; i < 4; ++i) {
        const std::int32_t s0 = 64 * c[i + 0] + 64 * c[i + 8];
        const std::int32_t s1 = 64 * c[i + 0] - 64 * c[i + 8];
        const std::int32_t o0 = 83 * c[i + 4] + 36 * c[i + 12];
        const std::int32_t o1 = 36 * c[i + 4] - 83 * c[i + 12];
        c[i + 0]  = clip_int16((s0 + o0 + (1 << (kInvShift1 - 1))) >> kInvShift1);
        c[i + 4]  = clip_int16((s1 + o1 + (1 << (kInvShift1 - 1))) >> kInvShift1);
        c[i + 8]  = clip_int16((s1 - o1 + (1 << (kInvShift1 - 1))) >> kInvShift1);
        c[i + 12] = clip_int16((s0 - o0 + (1 << (kInvShift1 - 1))) >> kInvShift1);
    }
}
void inv_dct1_rows(std::int32_t* c) {  // 行变换（step=1）
    for (int i = 0; i < 4; ++i) {
        const std::int32_t s0 = 64 * c[i * 4 + 0] + 64 * c[i * 4 + 2];
        const std::int32_t s1 = 64 * c[i * 4 + 0] - 64 * c[i * 4 + 2];
        const std::int32_t o0 = 83 * c[i * 4 + 1] + 36 * c[i * 4 + 3];
        const std::int32_t o1 = 36 * c[i * 4 + 1] - 83 * c[i * 4 + 3];
        c[i * 4 + 0] = clip_int16((s0 + o0 + (1 << (kInvShift2 - 1))) >> kInvShift2);
        c[i * 4 + 1] = clip_int16((s1 + o1 + (1 << (kInvShift2 - 1))) >> kInvShift2);
        c[i * 4 + 2] = clip_int16((s1 - o1 + (1 << (kInvShift2 - 1))) >> kInvShift2);
        c[i * 4 + 3] = clip_int16((s0 - o0 + (1 << (kInvShift2 - 1))) >> kInvShift2);
    }
}

// ffmpeg TR_4x4_LUMA（DST-VII 逆），assign = SCALE（两段均 av_clip_int16）。
// 精确复刻 ffmpeg transform_4x4_luma：先「列」(step=4, shift=7) 后「行」(step=1, shift=12)。
void inv_dst1_cols(std::int32_t* c) {  // 列变换（step=4）
    for (int i = 0; i < 4; ++i) {
        const std::int32_t c0 = c[i + 0] + c[i + 8];
        const std::int32_t c1 = c[i + 8] + c[i + 12];
        const std::int32_t c2 = c[i + 0] - c[i + 12];
        const std::int32_t c3 = 74 * c[i + 4];
        const std::int32_t a2 = 74 * (c[i + 0] - c[i + 8] + c[i + 12]);
        const std::int32_t a0 = 29 * c0 + 55 * c1 + c3;
        const std::int32_t a1 = 55 * c2 - 29 * c1 + c3;
        const std::int32_t a3 = 55 * c0 + 29 * c2 - c3;
        c[i + 8]  = clip_int16((a2 + (1 << (kInvShift1 - 1))) >> kInvShift1);
        c[i + 0]  = clip_int16((a0 + (1 << (kInvShift1 - 1))) >> kInvShift1);
        c[i + 4]  = clip_int16((a1 + (1 << (kInvShift1 - 1))) >> kInvShift1);
        c[i + 12] = clip_int16((a3 + (1 << (kInvShift1 - 1))) >> kInvShift1);
    }
}
void inv_dst1_rows(std::int32_t* c) {  // 行变换（step=1）
    for (int i = 0; i < 4; ++i) {
        const std::int32_t c0 = c[i * 4 + 0] + c[i * 4 + 2];
        const std::int32_t c1 = c[i * 4 + 2] + c[i * 4 + 3];
        const std::int32_t c2 = c[i * 4 + 0] - c[i * 4 + 3];
        const std::int32_t c3 = 74 * c[i * 4 + 1];
        const std::int32_t a2 = 74 * (c[i * 4 + 0] - c[i * 4 + 2] + c[i * 4 + 3]);
        const std::int32_t a0 = 29 * c0 + 55 * c1 + c3;
        const std::int32_t a1 = 55 * c2 - 29 * c1 + c3;
        const std::int32_t a3 = 55 * c0 + 29 * c2 - c3;
        c[i * 4 + 2] = clip_int16((a2 + (1 << (kInvShift2 - 1))) >> kInvShift2);
        c[i * 4 + 0] = clip_int16((a0 + (1 << (kInvShift2 - 1))) >> kInvShift2);
        c[i * 4 + 1] = clip_int16((a1 + (1 << (kInvShift2 - 1))) >> kInvShift2);
        c[i * 4 + 3] = clip_int16((a3 + (1 << (kInvShift2 - 1))) >> kInvShift2);
    }
}

// 单段正向 DST-VII 蝶形已弃用：编码器需产出「行主序」系数（与 ffmpeg idst 的原地布局一致），
// 故 DST 与 DCT 统一走 M·r·Mᵀ 矩阵法（见 fwd_transform_4x4）。

}  // namespace

void fwd_transform_4x4(const std::int16_t* residual, std::int32_t* coeff, bool use_dst) {
    // 可分 2D 变换 coeff = M·r·Mᵀ（HEVC 规范 8.6.2 整数核；DCT 按 64 缩放、DST-VII 按 128 缩放），
    // 两段 (x+add)>>shift：shift_1st=1、shift_2nd=8（x265 dct4_c/dst4_c）。产出「行主序」系数，
    // 与 ffmpeg 逆变换（先列 shift7、后行 shift12 的原地布局）直接对应。
    //   DCT：M 对称且 M²=2^14·I ⇒ fwd=M·r·M/2^9=32r，inv=M·c·M/2^19=c/32 ⇒ 往返精确 0 误差。
    //   DST：M 非对称，往返取整误差 ≤1；但编码器/解码器重建均走 inv（=ffmpeg idst），故解码一致。
    const std::int16_t(*M)[4] = use_dst ? kDst : kDct;
    std::int32_t tmp[16];
    const std::int64_t add1 = 1 << (1 - 1);  // shift_1st = 1
    for (int x = 0; x < 4; ++x)              // 段 1：对列施加 M（tmp = M·r）
        for (int i = 0; i < 4; ++i) {
            std::int64_t s = 0;
            for (int j = 0; j < 4; ++j) s += static_cast<std::int64_t>(M[i][j]) * residual[j * 4 + x];
            tmp[i * 4 + x] = static_cast<std::int32_t>((s + add1) >> 1);
        }
    const std::int64_t add2 = 1 << (8 - 1);  // shift_2nd = 8
    for (int y = 0; y < 4; ++y)              // 段 2：对每行 tmp 右乘 Mᵀ（coeff = M·r·Mᵀ）
        for (int x = 0; x < 4; ++x) {
            std::int64_t s = 0;
            for (int k = 0; k < 4; ++k) s += static_cast<std::int64_t>(M[x][k]) * tmp[y * 4 + k];
            coeff[y * 4 + x] = static_cast<std::int32_t>((s + add2) >> 8);
        }
}

void inv_transform_4x4(const std::int32_t* in, std::int16_t* residual, bool use_dst) {
    std::int32_t c[16];
    for (int i = 0; i < 16; ++i) c[i] = in[i];
    if (use_dst) {
        inv_dst1_cols(c);
        inv_dst1_rows(c);
    } else {
        inv_dct1_cols(c);
        inv_dct1_rows(c);
    }
    for (int i = 0; i < 16; ++i) residual[i] = static_cast<std::int16_t>(c[i]);
}

}  // namespace termchart::image::codec::hevc
