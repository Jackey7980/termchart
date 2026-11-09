// test_hevc_scan.cpp —— 帧内 4x4 TU 的三套系数扫描与逐 CU 模式决策回归
//
// 背景：FFmpeg hevcdec.c:1396-1403 由亮度模式隐式选 SCAN_DIAG/HORIZ/VERT，
// 三张扫描表、sig_coeff ctx 表、last_significant_coeff_x/y 的交换必须成套切换。
// 只测自往返不够（两侧一起错也会过），故同时锁表结构与模式→扫描的映射边界。
#include <doctest.h>

#include <array>
#include <cstdint>
#include <vector>

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
