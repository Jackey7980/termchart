// test_hevc_scan.cpp —— 帧内 4x4 TU 的三套系数扫描与逐 CU 模式决策回归
//
// 背景：FFmpeg hevcdec.c:1396-1403 由亮度模式隐式选 SCAN_DIAG/HORIZ/VERT，
// 三张扫描表、sig_coeff ctx 表、last_significant_coeff_x/y 的交换必须成套切换。
// 只测自往返不够（两侧一起错也会过），故同时锁表结构与模式→扫描的映射边界。
#include <doctest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "src/image/codec/hevc/cabac_context.hpp"
#include "src/image/codec/hevc/intra_prediction.hpp"
#include "src/image/codec/hevc/residual_coding.hpp"

namespace {

using namespace termchart::image::codec::hevc;

const std::array<std::int16_t, 16> kPatterns[4] = {
    // 单个 DC 系数
    std::array<std::int16_t, 16>{7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    // 稀疏多系数（含 last>0 与位置 0）
    std::array<std::int16_t, 16>{-3, 0, 2, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 5},
    // 密集系数 + 大幅值（触发 Rice escape 与 greater1/greater2）
    std::array<std::int16_t, 16>{12, -9, 4, -1, 3, 2, 18, -7, 1, 6, 2, 1, 9, -4, 2, 31},
    // 只有末位非零（last=15）
    std::array<std::int16_t, 16>{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -23},
};

}  // namespace

TEST_CASE("hevc: scan idx follows intra prediction mode (FFmpeg hevcdec.c:1396)") {
    for (int m : {0, 1, 2, 5, 15, 16, 17, 18, 21, 31, 32, 34})
        CHECK(scan_idx_for_intra_mode(m) == kScanDiag);
    for (int m = 6; m <= 14; ++m)
        CHECK(scan_idx_for_intra_mode(m) == kScanVert);
    for (int m = 22; m <= 30; ++m)
        CHECK(scan_idx_for_intra_mode(m) == kScanHoriz);
}

TEST_CASE("hevc: all three 4x4 scan tables are bijective and inverse-consistent") {
    for (ScanIdx idx : {kScanDiag, kScanHoriz, kScanVert}) {
        const auto& st = scan4x4_tables(idx);
        bool seen[4][4] = {};
        for (int n = 0; n < 16; ++n) {
            const int x = st.x[n], y = st.y[n];
            CHECK(x >= 0);
            CHECK(x < 4);
            CHECK(y >= 0);
            CHECK(y < 4);
            CHECK_FALSE(seen[y][x]);
            seen[y][x] = true;
            CHECK(st.inv[y][x] == n);
        }
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) CHECK(seen[y][x]);
    }
    CHECK(scan4x4_tables(kScanVert).swap_last_xy);
    CHECK_FALSE(scan4x4_tables(kScanDiag).swap_last_xy);
    CHECK_FALSE(scan4x4_tables(kScanHoriz).swap_last_xy);
}

TEST_CASE("hevc: residual syntax round-trips under DIAG/HORIZ/VERT") {
    for (ScanIdx idx : {kScanDiag, kScanHoriz, kScanVert}) {
        for (const auto& coeff : kPatterns) {
            bool ok = false;
            const auto bins = encode_residual_4x4(coeff.data(), &ok, idx);
            CHECK(ok);
            CHECK_FALSE(bins.empty());
            std::int16_t out[16] = {};
            std::size_t used = 0;
            CHECK(decode_residual_4x4(bins.data(), bins.size(), out, &used, idx));
            CHECK(used == bins.size());
            for (int n = 0; n < 16; ++n) CHECK(out[n] == coeff[n]);
        }
    }
}

TEST_CASE("hevc: intra mode decision is deterministic, in range, and content-aware") {
    constexpr int kSize = 64;
    std::vector<std::uint8_t> flat(kSize * kSize, 128);
    std::vector<std::uint8_t> ramp(kSize * kSize);
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x)
            ramp[y * kSize + x] = static_cast<std::uint8_t>(40 + (x + y) / 2);

    for (int qp : {22, 26, 34}) {
        const int flat_mode = choose_intra_luma_mode_8x8(
            flat.data(), kSize, flat.data(), kSize, 16, 16, kSize, kSize, qp);
        const int ramp_mode = choose_intra_luma_mode_8x8(
            ramp.data(), kSize, ramp.data(), kSize, 16, 16, kSize, kSize, qp);
        CHECK(flat_mode >= 0);
        CHECK(flat_mode <= 34);
        CHECK(ramp_mode >= 0);
        CHECK(ramp_mode <= 34);
        CHECK(choose_intra_luma_mode_8x8(
                  ramp.data(), kSize, ramp.data(), kSize, 16, 16, kSize, kSize, qp) == ramp_mode);
        CHECK((flat_mode == 0 || flat_mode == 1));  // 全平图：Planar/DC 等价
    }
    // 越界（x0+8 > pic_w）必须显式回退 INTRA_DC，不得越界读重建平面。
    CHECK(choose_intra_luma_mode_8x8(ramp.data(), kSize, ramp.data(), kSize,
                                     60, 16, kSize, kSize, 26) == 1);
}

TEST_CASE("hevc: bottom-left extension is disabled at each CTB row bottom") {
    // FFmpeg hevcdec.c:2753 的 end_of_tiles_y = FFMIN(y_ctb + ctb_size, height)，
    // mvs.c:58 据此在每个 CTB 行最后一行 TU 关闭 bottom-left 扩展。
    // 因此 CTB 首行最后一个 TU（y0=60, size=4 => y0+4 == 64）的预测
    // 绝不能受下一 CTB 行（y=64..71）重建内容的影响。
    constexpr int kStride = 128;
    constexpr int kH = 128;
    std::vector<std::uint8_t> rec(static_cast<std::size_t>(kStride) * kH, 40);
    std::uint8_t a[16] = {}, b[16] = {};

    for (int y = 64; y < 72; ++y)
        for (int x = 0; x < kStride; ++x)
            rec[static_cast<std::size_t>(y) * kStride + x] = 200;
    // mode 2 是正角度、用 L 参考，预测会读到 L[7]（bottom-left 扩展区）。
    predict_intra_luma_4x4(2, rec.data(), kStride, 8, 60, kStride, kH, a);

    for (int y = 64; y < 72; ++y)
        for (int x = 0; x < kStride; ++x)
            rec[static_cast<std::size_t>(y) * kStride + x] = 0;
    predict_intra_luma_4x4(2, rec.data(), kStride, 8, 60, kStride, kH, b);

    for (int i = 0; i < 16; ++i) CHECK(a[i] == b[i]);
}

TEST_CASE("hevc: chroma residual uses c_idx>0 CABAC offsets and round-trips") {
    // FFmpeg cabac.c：4x4 色度 TU 的 last_sig x/y 用 ctx_offset=15（亮度 0）。
    // 只测自往返不够（两侧一起错也会过），必须同时锁绝对 ctx 值。
    for (const auto& p : kPatterns) {
        for (int c_idx : {1, 2}) {
            bool ok = false;
            const auto bins = encode_residual_4x4(p.data(), &ok, kScanDiag, c_idx);
            CHECK(ok);
            CHECK_FALSE(bins.empty());
            CHECK(bins.front().ctx >= kCtxLastSigCoeffX + 15);
            CHECK(bins.front().ctx <= kCtxLastSigCoeffX + 15 + 3);
            std::int16_t back[16] = {};
            std::size_t used = 0;
            CHECK(decode_residual_4x4(bins.data(), bins.size(), back, &used,
                                      kScanDiag, c_idx));
            CHECK(used == bins.size());
            for (int i = 0; i < 16; ++i) CHECK(back[i] == p[i]);
        }
        // 同一份系数：亮度首 bin 不能误用色度偏移
        const auto luma = encode_residual_4x4(p.data(), nullptr, kScanDiag, 0);
        const auto chroma = encode_residual_4x4(p.data(), nullptr, kScanDiag, 1);
        CHECK(luma.front().ctx == chroma.front().ctx - 15);
    }
}

TEST_CASE("hevc: chroma prediction skips luma-only DC post-filter") {
    // FFmpeg pred_template.c 的 DC 首行/首列 3*dc+2 与 mode 10/26 后处理
    // 都带 c_idx==0 条件。色度参考非平坦时，整块输出必须恒等于 dc。
    constexpr int kStride = 16;
    std::vector<std::uint8_t> plane(kStride * kStride, 128);
    for (int x = 0; x < kStride; ++x)
        plane[static_cast<std::size_t>(kStride) + x] = 200;  // 色度参考行 y=1
    for (int y = 0; y < kStride; ++y)
        plane[static_cast<std::size_t>(y) * kStride + 1] = 60;  // 参考列 x=1
    plane[static_cast<std::size_t>(kStride) + 1] = 100;
    std::uint8_t dst[16] = {};
    predict_intra_luma_4x4(1, plane.data(), kStride, 4, 4,
                           kStride * 2, kStride * 2, dst,
                           ChromaPredPlane{plane.data(), kStride});
    for (int i = 1; i < 16; ++i) CHECK(dst[i] == dst[0]);
}
