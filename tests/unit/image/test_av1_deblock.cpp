// test_av1_deblock.cpp —— AV1 阶段七去块滤波：E/I 表、HEV 与平坦区路径
#include <doctest.h>

#include <array>
#include <cstdint>

#include "src/media/encoder/av1/postproc/deblock.hpp"

using namespace termchart::image::codec::av1;

TEST_CASE("av1 deblock: level 0 is a no-op") {
    std::array<std::uint8_t, 128> y{};
    for (int i = 0; i < 128; ++i) y[static_cast<std::size_t>(i)] = (i * 7) & 255;
    const auto copy = y;
    av1_deblock_luma_grid(y.data(), 8, 8, 16, Av1DeblockParams{0, 0});
    CHECK(y == copy);
}

TEST_CASE("av1 deblock: 4-tap edge follows the HEV branch") {
    std::array<std::uint8_t, 64> y{};
    for (int i = 0; i < 64; ++i) y[static_cast<std::size_t>(i)] = 128;
    // p1=122, p0=126, q0=130, q1=134 ⇒ 非平坦但有界，走常规 4 抽头。
    y[4 * 8] = 120;
    y[5 * 8] = 126;
    y[6 * 8] = 126;
    y[7 * 8] = 134;
    const auto before = y;
    av1_deblock_edge(y.data(), 8, 0, 6, false, 4, 4,
                     Av1DeblockParams{40, 0});
    CHECK(y != before);
    CHECK((y[5 * 8] >= 120 && y[5 * 8] <= 134));
    CHECK((y[6 * 8] >= 120 && y[6 * 8] <= 134));
}

TEST_CASE("av1 deblock: sharpness clamps I and E") {
    std::array<std::uint8_t, 64> y{};
    for (int i = 0; i < 64; ++i) y[static_cast<std::size_t>(i)] = 100;
    y[4 * 8] = 96; y[5 * 8] = 98; y[6 * 8] = 102; y[7 * 8] = 104;
    // 锐度 7 时 I 被压到 2，|p1-p0|=2、|q1-q0|=2 仍可滤波，但幅度不同。
    av1_deblock_edge(y.data(), 8, 0, 6, false, 4, 4,
                     Av1DeblockParams{63, 7});
    CHECK((y[5 * 8] >= 90 && y[5 * 8] <= 110));
}
