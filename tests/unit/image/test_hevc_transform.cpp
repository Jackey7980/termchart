// test_hevc_transform.cpp —— HEVC 整数变换/量化层 4/8/16/32 + transform_skip 回归
//
// 为什么需要外部 oracle：本库曾出现过「fwd→inv 自往返全绿、但与 FFmpeg 解码不一致」
// 的事故（见 docs/reference/hevc_engineering_roadmap.md §2.4：自往返测试不能替代外部
// oracle）。2026-11-10 又在 inv_transform_large 发现 in-place 别名污染：pass1 边读
// buf 边写 buf，k 递增时后续迭代读到被覆盖的同列值，导致 8/16/32 往返增益逐行衰减
// （0.51/0.36/0.44），而 4x4 蝶形因先读局部量再写一直正常。
//
// 因此本文件用两条独立路径交叉锁定：
//   (1) ffmpeg 蝶形 oracle —— 逐字展开 libavcodec/hevc/dsp_template.c 的
//       TR_4/8/16/32 + IDCT 两级（列 shift7、行 shift12、每级 clip_int16），
//       只用 ffmpeg 的 transform[32][32] 表（kTr32），不经过本库的 g_at/矩阵乘；
//   (2) fwd_transform → inv_transform 整数往返，以及 quant/encode_tu 全链路。
#include <doctest.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstdint>

#include "src/media/encoder/hevc/quantize.hpp"
#include "src/media/encoder/hevc/transform.hpp"
#include "src/media/encoder/hevc/transform_table.hpp"
#include "src/media/encoder/hevc/transquant.hpp"

namespace {

using namespace termchart::image::codec::hevc;

constexpr int kMaxBlock = 32 * 32;

inline std::int16_t clip16(std::int64_t v) {
    return static_cast<std::int16_t>(std::clamp<std::int64_t>(v, -32768, 32767));
}

// ── ffmpeg dsp_template.c 的 TR_N 蝶形（mode=0 为内部 SET，mode=1 为顶层 SCALE）──
void ffmpeg_butterfly(std::int32_t* dst, const std::int32_t* src, int dstep,
                      int sstep, int n, int mode, int add, int shift) {
    const int half = n / 2;
    const int stride = 32 / n;
    std::int32_t e[16] = {0}, o[16] = {0};
    if (n == 4) {
        const std::int32_t e0 = 64 * src[0] + 64 * src[2 * sstep];
        const std::int32_t e1 = 64 * src[0] - 64 * src[2 * sstep];
        const std::int32_t o0 = 83 * src[1 * sstep] + 36 * src[3 * sstep];
        const std::int32_t o1 = 36 * src[1 * sstep] - 83 * src[3 * sstep];
        e[0] = e0; e[1] = e1; o[0] = o0; o[1] = o1;
        for (int i = 0; i < 2; ++i) {
            const std::int64_t a = e[i] + o[i], b = e[i] - o[i];
            dst[i * dstep] = mode ? clip16((a + add) >> shift) : static_cast<std::int32_t>(a);
            dst[(3 - i) * dstep] = mode ? clip16((b + add) >> shift) : static_cast<std::int32_t>(b);
        }
        return;
    }
    for (int i = 0; i < half; ++i)
        for (int j = 1; j < n; j += 2)
            o[i] += static_cast<std::int32_t>(kTr32[stride * j][i]) * src[j * sstep];
    ffmpeg_butterfly(e, src, 1, 2 * sstep, n / 2, 0, 0, 0);
    for (int i = 0; i < half; ++i) {
        const std::int64_t a = static_cast<std::int64_t>(e[i]) + o[i];
        const std::int64_t b = static_cast<std::int64_t>(e[i]) - o[i];
        dst[i * dstep] = mode ? clip16((a + add) >> shift) : static_cast<std::int32_t>(a);
        dst[(n - 1 - i) * dstep] = mode ? clip16((b + add) >> shift) : static_cast<std::int32_t>(b);
    }
}

// ffmpeg idct_NxN(coeffs, col_limit=N)：第一遍逐列（SCALE shift7），第二遍逐行（SCALE shift12）
void ffmpeg_idct(const std::int32_t* coeff, std::int16_t* out, int log2_size) {
    const int n = 1 << log2_size;
    std::int32_t buf[kMaxBlock];
    for (int i = 0; i < n * n; ++i) buf[i] = coeff[i];
    for (int x = 0; x < n; ++x) {
        std::int32_t col[32], res[32];
        for (int k = 0; k < n; ++k) col[k] = buf[k * n + x];
        ffmpeg_butterfly(res, col, 1, 1, n, 1, 64, 7);
        for (int k = 0; k < n; ++k) buf[k * n + x] = res[k];
    }
    for (int y = 0; y < n; ++y) {
        std::int32_t row[32], res[32];
        for (int k = 0; k < n; ++k) row[k] = buf[y * n + k];
        ffmpeg_butterfly(res, row, 1, 1, n, 1, 2048, 12);
        for (int k = 0; k < n; ++k) out[y * n + k] = clip16(res[k]);
    }
}

std::uint32_t g_rng = 0x5eed1234u;
int rnd(int lo, int hi) {
    g_rng = g_rng * 1664525u + 1013904223u;
    return lo + static_cast<int>((g_rng >> 8) % static_cast<unsigned>(hi - lo + 1));
}

int maxdiff_i16(const std::int16_t* a, const std::int16_t* b, int count) {
    int d = 0;
    for (int i = 0; i < count; ++i)
        d = std::max(d, std::abs(static_cast<int>(a[i]) - static_cast<int>(b[i])));
    return d;
}

}  // namespace

// ── 1. 逆变换与 ffmpeg 蝶形逐位一致（外部 oracle，4/8/16/32）─────────────────
TEST_CASE("hevc: inv_transform matches ffmpeg TR_N butterfly bitwise (4/8/16/32)") {
    g_rng = 0xA1B2C3D4u;
    for (int ls = 2; ls <= 5; ++ls) {
        const int n = 1 << ls, cnt = n * n;
        int worst = 0;
        for (int trial = 0; trial < 24; ++trial) {
            const int amp = (trial % 3 == 0) ? 3000 : ((trial % 3 == 1) ? 300 : 20);
            std::int32_t coeff[kMaxBlock];
            std::int16_t got[kMaxBlock], ref[kMaxBlock];
            for (int i = 0; i < cnt; ++i) coeff[i] = rnd(-amp, amp);
            inv_transform(coeff, got, ls, false, n);  // col_limit = n：编码端不截断
            ffmpeg_idct(coeff, ref, ls);
            worst = std::max(worst, maxdiff_i16(got, ref, cnt));
        }
        // FFmpeg 逐位一致是硬约束：任何偏差都会让编码端重建基准与解码端漂移
        CHECK(worst == 0);
    }
}

// ── 2. fwd→inv 无量化整数往返（锁死 in-place 别名 bug）──────────────────────
TEST_CASE("hevc: fwd/inv round-trip is near-lossless for 4/8/16/32 (in-place aliasing guard)") {
    g_rng = 0x5A5A5A5Au;
    for (int ls = 2; ls <= 5; ++ls) {
        const int n = 1 << ls, cnt = n * n;
        // (a) 每个位置放单脉冲：别名污染曾让增益随行号衰减到 0.03，逐点全覆盖可捕获
        for (int py = 0; py < n; ++py) {
            for (int px = 0; px < n; ++px) {
                std::int16_t r[kMaxBlock] = {}, back[kMaxBlock];
                std::int32_t coeff[kMaxBlock];
                r[py * n + px] = 100;
                fwd_transform(r, coeff, ls, false);
                inv_transform(coeff, back, ls, false, n);
                CHECK(back[py * n + px] == 100);
            }
        }
        // (b) 一般随机信号：两级 clip_int16 + 移位只有几个 LSB 的舍入误差。
        // in-place 别名 bug 时误差是几十的量级（增益衰减到 0.5x/0.36x），阈值 3 仍能死锁。
        int worst = 0;
        for (int trial = 0; trial < 12; ++trial) {
            std::int16_t r[kMaxBlock], back[kMaxBlock];
            std::int32_t coeff[kMaxBlock];
            for (int i = 0; i < cnt; ++i) r[i] = static_cast<std::int16_t>(rnd(-90, 90));
            fwd_transform(r, coeff, ls, false);
            inv_transform(coeff, back, ls, false, n);
            worst = std::max(worst, maxdiff_i16(r, back, cnt));
        }
        CHECK(worst <= 3);
    }
}

// ── 3. 4x4 已验证路径不回归：DCT-II 与 DST-VII 各自往返 + 兼容包装等价 ──────
TEST_CASE("hevc: 4x4 DCT/DST paths and compatibility wrappers stay intact") {
    for (bool use_dst : {false, true}) {
        std::int16_t r[16] = {12, -30, 7, 0, -5, 22, -11, 3, 8, -2, 19, -7, 4, 15, -9, 6};
        std::int16_t back[16], back2[16];
        std::int32_t coeff[16], coeff2[16];
        fwd_transform(r, coeff, 2, use_dst);
        inv_transform(coeff, back, 2, use_dst, 4);
        CHECK(maxdiff_i16(r, back, 16) <= 1);

        fwd_transform_4x4(r, coeff2, use_dst);
        inv_transform_4x4(coeff2, back2, use_dst);
        for (int i = 0; i < 16; ++i) {
            CHECK(coeff[i] == coeff2[i]);
            CHECK(back[i] == back2[i]);
        }
    }
    // DST 与 DCT 是不同变换核（除非残差恰为 DC）
    std::int16_t r[16] = {12, -30, 7, 0, -5, 22, -11, 3, 8, -2, 19, -7, 4, 15, -9, 6};
    std::int32_t cdct[16], cdst[16];
    fwd_transform(r, cdct, 2, false);
    fwd_transform(r, cdst, 2, true);
    bool any_diff = false;
    for (int i = 0; i < 16; ++i) any_diff = any_diff || (cdct[i] != cdst[i]);
    CHECK(any_diff);
}

// ── 4. quantize/dequant size-aware + transform_skip 分支 ────────────────────
TEST_CASE("hevc: quant shift scales with log2_size and transform_skip removes it") {
    std::int32_t coeff[kMaxBlock];
    for (int i = 0; i < kMaxBlock; ++i) coeff[i] = 100000;
    std::int16_t lvl[kMaxBlock];
    // 非 skip：shift = 8 + log2_size - 5，同一 coeff 的 level 随尺寸严格递增
    int prev = -1;
    for (int ls = 2; ls <= 5; ++ls) {
        quant(coeff, lvl, 26, ls, false);
        CHECK(lvl[0] > prev);
        prev = lvl[0];
    }
    // skip：净移位 = (3+log2)+(7-log2) = 10，与尺寸无关。常数 coeff 下每个尺寸
    // 整块 level 应相同，且各尺寸 DC 值彼此相等。（不能用 ls=2 的 16 元素数组去比
    // 更大尺寸，会读到未初始化内存。）
    std::int16_t skip_dc = -1;
    for (int ls = 2; ls <= 5; ++ls) {
        quant(coeff, lvl, 26, ls, true);
        const int cnt = 1 << (2 * ls);
        CHECK(lvl[0] > 0);
        bool uniform = true;
        for (int i = 0; i < cnt; ++i) uniform = uniform && (lvl[i] == lvl[0]);
        CHECK(uniform);
        if (skip_dc < 0) skip_dc = lvl[0];
        CHECK(lvl[0] == skip_dc);
    }
    // skip 与非 skip 的 shift 不同 → level 不同（用布尔量避开 doctest 表达式分解）
    std::int16_t nonskip[kMaxBlock];
    quant(coeff, nonskip, 26, 2, false);
    const bool differs = (skip_dc != nonskip[0]);
    CHECK(differs);
}

TEST_CASE("hevc: dequant preserves sign and is monotonic; QP monotonically shrinks levels") {
    std::int16_t lvl[kMaxBlock];
    std::int32_t dq[kMaxBlock];
    for (int ls = 2; ls <= 5; ++ls) {
        const int cnt = 1 << (2 * ls);
        for (int i = 0; i < cnt; ++i) lvl[i] = static_cast<std::int16_t>(i - cnt / 2);
        dequant(lvl, dq, 20, ls, false);
        for (int i = 0; i < cnt; ++i) {
            const int v = i - cnt / 2;
            const bool sign_ok = (v == 0) ? (dq[i] == 0)
                                          : ((v > 0) ? (dq[i] > 0) : (dq[i] < 0));
            CHECK(sign_ok);
        }
        // QP 越大，量化步长越大 → 非零 level 个数单调不增；level 幅值单调不增。
        std::int32_t c[kMaxBlock];
        for (int i = 0; i < cnt; ++i) c[i] = (i % 7) * 137 - 400;
        int prev_nz = cnt + 1;
        int prev_max = INT32_MAX;
        for (int qp : {0, 12, 26, 40, 51}) {
            const int nz = quant(c, lvl, qp, ls, false);
            int mx = 0;
            for (int i = 0; i < cnt; ++i) mx = std::max(mx, std::abs(static_cast<int>(lvl[i])));
            CHECK(nz <= prev_nz);
            CHECK(mx <= prev_max);
            prev_nz = nz;
            prev_max = mx;
        }
    }
}

// ── 4b. transform_skip 逆量化的 ffmpeg 两段式 oracle（4/8/16/32 × 全 QP）────
// 逐字复刻 libavcodec/hevc/cabac.c:1556（段1，含 clip16）与
// dsp_template.c:127 FUNC(dequant)（段2）。这是外部真值，不能用自往返替代：
// 旧实现 shift=3 时编码端 quant/dequant 自洽（单测全绿），但 ffmpeg 解码净 >>10，
// 重建像素会差 128 倍 —— 自往返测试完全测不出来。
TEST_CASE("hevc: transform_skip dequant matches ffmpeg two-stage oracle bitwise") {
    constexpr std::uint8_t kLevelScale[6] = {40, 45, 51, 57, 64, 72};  // cabac.c:1114
    for (int ls = 2; ls <= 5; ++ls) {
        const int cnt = 1 << (2 * ls);
        const int s1 = 8 + ls - 5;    // cabac.c:1173 bit_depth + log2 - 5
        const int s2 = 15 - 8 - ls;   // dsp_template.c:129 15 - BIT_DEPTH - log2
        REQUIRE(s2 > 0);
        for (int qp : {0, 7, 12, 26, 40, 51}) {
            const int rem = qp % 6, div = qp / 6;
            const std::int64_t scale = static_cast<std::int64_t>(kLevelScale[rem]) << div;
            const std::int64_t add1 = 1LL << (s1 - 1);
            const std::int32_t add2 = 1 << (s2 - 1);
            std::int16_t lvl[kMaxBlock];
            std::int32_t got[kMaxBlock];
            for (int i = 0; i < cnt; ++i) lvl[i] = static_cast<std::int16_t>(rnd(-2000, 2000));
            dequant(lvl, got, qp, ls, true);
            int diff = 0;
            for (int i = 0; i < cnt; ++i) {
                // 段1：clip16((level*scale*16 + add1) >> s1)
                std::int64_t t = (static_cast<std::int64_t>(lvl[i]) * scale * 16 + add1) >> s1;
                std::int32_t tc = static_cast<std::int32_t>(
                    std::clamp<std::int64_t>(t, -32768, 32767));
                // 段2：int16 域 (tc + add2) >> s2，随后 clip16
                const std::int32_t r = (tc + add2) >> s2;
                const std::int32_t ref = static_cast<std::int32_t>(
                    std::clamp<std::int64_t>(r, -32768, 32767));
                diff = std::max(diff, std::abs(got[i] - ref));
            }
            CHECK(diff == 0);
        }
    }
}

// ── 5. encode_tu / reconstruct_tu 全尺寸 + transform_skip 重建自洽 ──────────
TEST_CASE("hevc: encode_tu reconstruct equals standalone reconstruct_tu (4/8/16/32 + skip)") {
    g_rng = 0x13572468u;
    for (int ls = 2; ls <= 5; ++ls) {
        const int n = 1 << ls;
        for (bool skip : {false, true}) {
            std::uint8_t src[kMaxBlock], pred[kMaxBlock], rec[kMaxBlock], rec2[kMaxBlock];
            std::int16_t level[kMaxBlock];
            for (int i = 0; i < n * n; ++i) {
                src[i] = static_cast<std::uint8_t>(rnd(0, 255));
                pred[i] = static_cast<std::uint8_t>(rnd(0, 255));
            }
            // 低 QP 保证 skip 分支也产生非零 level（真正走通 skip 量化路径）
            const int nz = encode_tu(src, n, pred, rec, n, level, 0, ls, false, skip);
            reconstruct_tu(level, 0, pred, rec2, n, ls, false, skip);
            int d = 0;
            for (int i = 0; i < n * n; ++i)
                d = std::max(d, std::abs(static_cast<int>(rec[i]) - static_cast<int>(rec2[i])));
            CHECK(d == 0);
            CHECK(nz > 0);
        }
    }
}

TEST_CASE("hevc: encode_tu_4x4 wrapper matches size-aware encode_tu") {
    const int n = 4;
    std::uint8_t src[16], pred[16], rec[16], rec2[16];
    std::int16_t level[16], level2[16];
    for (int i = 0; i < 16; ++i) {
        src[i] = static_cast<std::uint8_t>(30 + i * 11);
        pred[i] = static_cast<std::uint8_t>((i * 7) % 200);
    }
    const int a = encode_tu_4x4(src, n, pred, rec, n, level, 26, true);
    const int b = encode_tu(src, n, pred, rec2, n, level2, 26, 2, true, false);
    CHECK(a == b);
    for (int i = 0; i < 16; ++i) {
        CHECK(level[i] == level2[i]);
        CHECK(rec[i] == rec2[i]);
    }
}

// ── 6. inverse_col_limit 的公式边界（ffmpeg cabac.c:1272-1279）─────────────
TEST_CASE("hevc: inverse_col_limit follows ffmpeg caps and clamps to block size") {
    for (int ls = 2; ls <= 5; ++ls) {
        const int n = 1 << ls;
        CHECK(inverse_col_limit(0, 0, ls) <= 4);       // last<4 → min(4, …)
        CHECK(inverse_col_limit(0, 0, ls) >= 4);       // = 4
        CHECK(inverse_col_limit(5, 5, ls) <= 8);       // last<8 → min(8, …)
        CHECK(inverse_col_limit(9, 9, ls) <= 24);      // last<12 → min(24, …)
        CHECK(inverse_col_limit(n, n, ls) <= n);       // 饱和到块尺寸
    }
    // 4x4 恒为 4（兼容 inv_transform_4x4 的 col_limit=4）
    CHECK(inverse_col_limit(3, 3, 2) == 4);
}
