// test_av1_stage78.cpp —— AV1 阶段七 CDEF/LR、阶段八 RDO 决策核心
#include <doctest.h>

#include <array>
#include <cstdint>

#include "src/image/codec/av1/postproc/cdef.hpp"
#include "src/image/codec/av1/postproc/loop_restoration.hpp"
#include "src/image/codec/av1/rdo/rdo.hpp"

using namespace termchart::image::codec::av1;

TEST_CASE("av1 cdef: direction search and strength-zero no-op") {
    std::array<std::uint8_t, 64> img{};
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            img[static_cast<std::size_t>(y * 8 + x)] =
                static_cast<std::uint8_t>(32 + x * 6 + y * 2);
    unsigned var = 0;
    const int dir = av1_cdef_find_dir(img.data(), 8, &var);
    CHECK(dir >= 0);
    CHECK(dir < 8);
    CHECK(var > 0);

    const auto src = img;
    av1_cdef_filter_8x8(img.data(), 8, src.data(), 8, dir,
                        Av1CdefParams{0, 0, 5}, 15);
    CHECK(img == src);
}

TEST_CASE("av1 cdef: filtering stays inside the min/max neighborhood") {
    std::array<std::uint8_t, 64> img{};
    for (int i = 0; i < 64; ++i) img[static_cast<std::size_t>(i)] = 100;
    img[3 * 8 + 3] = 180;
    const auto src = img;
    av1_cdef_filter_8x8(img.data(), 8, src.data(), 8, 0,
                        Av1CdefParams{24, 8, 3}, 15);
    for (int i = 0; i < 64; ++i) CHECK(img[static_cast<std::size_t>(i)] <= 180);
    CHECK(img[3 * 8 + 3] <= 180);
}

TEST_CASE("av1 loop restoration: zero weights are no-ops") {
    std::array<std::uint8_t, 64> src{}, dst{};
    for (int i = 0; i < 64; ++i) src[static_cast<std::size_t>(i)] = (i * 13) & 255;
    av1_lr_wiener_unit(dst.data(), 8, src.data(), 8, 8, 8, Av1WienerParams{});
    CHECK(dst == src);
    dst.fill(0);
    av1_lr_sgrproj_unit(dst.data(), 8, src.data(), 8, 8, 8, Av1SgrParams{});
    CHECK(dst == src);
}

TEST_CASE("av1 loop restoration: nonzero taps smooth a single impulse") {
    std::array<std::uint8_t, 64> src{}, dst{};
    src.fill(128);
    src[4 * 8 + 4] = 200;
    Av1WienerParams wp;
    wp.h[0] = -3; wp.h[1] = 4; wp.h[2] = 12;
    wp.v[0] = -2; wp.v[1] = 5;
    av1_lr_wiener_unit(dst.data(), 8, src.data(), 8, 8, 8, wp);
    CHECK(dst[4 * 8 + 4] < 200);
}

TEST_CASE("av1 rdo: symbol bits, SSD and mode decision") {
    const std::uint16_t cdf[3] = {24576, 32768, 32768};
    CHECK(av1_rdo_symbol_bits(cdf, 2, 0) < av1_rdo_symbol_bits(cdf, 2, 1));

    std::array<std::uint8_t, 64> src{}, a{}, b{};
    src.fill(100);
    a.fill(100);
    b.fill(120);
    CHECK(av1_rdo_sse(src.data(), a.data(), 64) == 0);
    CHECK(av1_rdo_sse(src.data(), b.data(), 64) == 64u * 20u * 20u);

    std::array<std::uint8_t, 128> cands{};
    for (int i = 0; i < 64; ++i) {
        cands[static_cast<std::size_t>(i)] = a[static_cast<std::size_t>(i)];
        cands[static_cast<std::size_t>(64 + i)] = b[static_cast<std::size_t>(i)];
    }
    const auto best = av1_rdo_pick_mode(src.data(), cands.data(), 2, cdf, 1.0, true);
    CHECK(best.mode == 0);
    CHECK(best.distortion == 0);
    const auto greedy = av1_rdo_pick_mode(src.data(), cands.data(), 2, cdf, 1.0, false);
    CHECK(greedy.mode == best.mode);

    // 整块预测完美时应拒绝划分；四块预测完美时应接受。
    std::array<std::uint8_t, 64> parts{};
    parts.fill(100);
    CHECK_FALSE(av1_rdo_should_split(src.data(), a.data(), parts.data(), 64, 1.0));
    parts.fill(90);
    CHECK(av1_rdo_should_split(src.data(), b.data(), parts.data(), 64, 1.0));
}
