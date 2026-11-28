// test_av1_transform.cpp —— AV1 阶段六：全尺寸变换、扫描序与量化矩阵回归
#include <doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>

#include "src/media/encoder/av1/transform/transform_2d.hpp"

using namespace termchart::image::codec::av1;

namespace {

std::int32_t signal_at(const std::int16_t* coeff, int w, int h,
                       Av1Transform1d tx, Av1Transform1d ty, int i) {
    std::array<std::int32_t, 4096> out{};
    av1_inverse_transform_2d(coeff, out.data(), w, h, tx, ty);
    return (out[static_cast<std::size_t>(i)] + 8) >> 4;
}

}  // namespace

TEST_CASE("av1 transform: DCT8 DC matches the bit-exact IDCT8 kernel") {
    std::array<std::int16_t, 64> coeff{};
    coeff[0] = 256;
    // 单个 DC 系数经两遍 1D IDCT8（中间 >>1、末尾 >>4）得到常数残差 4。
    for (int i = 0; i < 64; ++i)
        CHECK(signal_at(coeff.data(), 8, 8, Av1Transform1d::Dct,
                        Av1Transform1d::Dct, i) == 4);
}

TEST_CASE("av1 transform: identity roundtrip on every block size") {
    for (int n : {2, 4, 8, 16, 32, 64}) {
        std::array<std::int32_t, 4096> residual{};
        for (int i = 0; i < n * n; ++i)
            residual[static_cast<std::size_t>(i)] = ((i * 37) % 97) - 48;
        std::array<std::int16_t, 4096> coeff{};
        av1_forward_transform_2d(residual.data(), coeff.data(), n, n,
                                 Av1Transform1d::Identity,
                                 Av1Transform1d::Identity);
        std::array<std::int32_t, 4096> out{};
        av1_inverse_transform_2d(coeff.data(), out.data(), n, n,
                                 Av1Transform1d::Identity,
                                 Av1Transform1d::Identity);
        for (int i = 0; i < n * n; ++i) {
            const int got = (out[static_cast<std::size_t>(i)] + 8) >> 4;
            CHECK(std::abs(got - residual[static_cast<std::size_t>(i)]) <= 1);
        }
    }
}

TEST_CASE("av1 transform: DCT/ADST roundtrip and rectangular blocks") {
    const Av1Transform1d kinds[] = {Av1Transform1d::Dct, Av1Transform1d::Adst,
                                    Av1Transform1d::FlipAdst};
    for (int w : {4, 8, 16}) {
        for (int h : {4, 8, 16}) {
            for (Av1Transform1d tx : kinds) {
                for (Av1Transform1d ty : kinds) {
                    std::array<std::int32_t, 256> residual{};
                    for (int i = 0; i < w * h; ++i)
                        residual[static_cast<std::size_t>(i)] =
                            ((i * 29) % 61) - 30;
                    std::array<std::int16_t, 256> coeff{};
                    av1_forward_transform_2d(residual.data(), coeff.data(), w, h,
                                             tx, ty);
                    std::array<std::int32_t, 256> out{};
                    av1_inverse_transform_2d(coeff.data(), out.data(), w, h,
                                             tx, ty);
                    for (int i = 0; i < w * h; ++i) {
                        const int got = (out[static_cast<std::size_t>(i)] + 8) >> 4;
                        CHECK(std::abs(got - residual[static_cast<std::size_t>(i)]) <= 3);
                    }
                }
            }
        }
    }
}

TEST_CASE("av1 transform: three scan orders are permutations at all sizes") {
    for (int w : {2, 4, 8, 16, 32, 64}) {
        for (int h : {2, 4, 8, 16, 32, 64}) {
            for (int scan = 0; scan < 3; ++scan) {
                std::array<int, 4096> rc{};
                av1_scan_order(w, h, scan, rc.data());
                std::array<bool, 4096> seen{};
                for (int i = 0; i < w * h; ++i) {
                    const int idx = rc[static_cast<std::size_t>(i)];
                    CHECK(idx >= 0);
                    CHECK(idx < w * h);
                    CHECK_FALSE(seen[static_cast<std::size_t>(idx)]);
                    seen[static_cast<std::size_t>(idx)] = true;
                }
            }
        }
    }
}

TEST_CASE("av1 quant matrix: 15 presets and dequant scaling") {
    auto prev = av1_qmatrix_8x8(0, false);
    for (int level = 1; level < 15; ++level) {
        const auto qm = av1_qmatrix_8x8(level, false);
        for (int i = 0; i < 64; ++i) {
            CHECK(qm[static_cast<std::size_t>(i)] > 0);
            CHECK(qm[static_cast<std::size_t>(i)] <= 255);
            CHECK(qm[static_cast<std::size_t>(i)] <= prev[static_cast<std::size_t>(i)]);
        }
        prev = qm;
    }
    // 8x8 表是规范独立预设（不是 4x4 的简单复制），DC 处亮度/色度均在 16..255。
    CHECK(av1_qmatrix_8x8(0, false)[0] >= 16);
    CHECK(av1_qmatrix_8x8(0, true)[0] <= 255);
    // 越界 level 钳到合法范围（15 = 不用 qmatrix，权重 32）。
    const auto clamped = av1_qmatrix_8x8(15, false);
    CHECK(clamped[0] == 32);
}
