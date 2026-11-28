// test_av1_intra_edge.cpp —— AV1 帧内边界预处理（规范 7.11.2.9 / 7.11.2.10）回归测试
//
// 期望值直接按规范表格/公式转写，不依赖实现内部推导：
//  * Intra_Edge_Filter_Strength 的 type 0（亮度）/ type 1（色度或邻居含 SMOOTH 族）；
//  * Intra_Edge_Upsample 的触发条件；
//  * 三档 3 抽头核 {0,4,8,4,0}/{0,5,6,5,0}/{2,4,4,4,2}，除数 16、偏移 8；
//  * 角点 5/6/5 滤波、半样本上采样的 9/9/-1/-1 插值。
//
// type 1 之所以对亮度块也生效，是因为解码端会看邻居模式：上/左邻居只要有一个是
// SMOOTH 族，亮度块也按 type 1 的阈值处理（libaom get_intra_edge_filter_type）。
// 8x16 纵切片里下块的邻居恰好是 SMOOTH_PRED 块，漏掉这个耦合会让方向预测差 1..2 级。
#include <doctest.h>

#include <array>
#include <cstdint>

#include "src/media/encoder/av1/predict/intra_edge.hpp"

namespace {

using namespace termchart::image::codec::av1;

struct StrengthCase {
    int bs0;
    int bs1;
    int delta;
    int type;
    int expected;
};

// 每个分支的阈值上下各取一个点（含负角度验证 Abs）。
constexpr StrengthCase kStrengthCases[] = {
    // type 0：blk_wh <= 8 时 d >= 56
    {4, 4, 55, 0, 0},
    {4, 4, 56, 0, 1},
    {4, 4, -56, 0, 1},
    // type 0：blk_wh <= 12 时 d >= 40
    {6, 6, 39, 0, 0},
    {6, 6, 40, 0, 1},
    // type 0：blk_wh <= 16 时 d >= 40
    {8, 8, 39, 0, 0},
    {8, 8, 40, 0, 1},
    // type 0：blk_wh <= 24（=20）三级 8 / 16 / 32
    {10, 10, 7, 0, 0},
    {10, 10, 8, 0, 1},
    {10, 10, 15, 0, 1},
    {10, 10, 16, 0, 2},
    {10, 10, 31, 0, 2},
    {10, 10, 32, 0, 3},
    // type 0：blk_wh <= 32（=28）三级 1 / 4 / 32
    {14, 14, 0, 0, 0},
    {14, 14, 1, 0, 1},
    {14, 14, 4, 0, 2},
    {14, 14, 31, 0, 2},
    {14, 14, 32, 0, 3},
    // type 0：blk_wh > 32 时 d >= 1 即 3
    {20, 20, 0, 0, 0},
    {20, 20, 1, 0, 3},
    // type 1：blk_wh <= 8 时 40 / 64
    {4, 4, 39, 1, 0},
    {4, 4, 40, 1, 1},
    {4, 4, 63, 1, 1},
    {4, 4, 64, 1, 2},
    // type 1：blk_wh <= 16 时 20 / 48
    {8, 8, 19, 1, 0},
    {8, 8, 20, 1, 1},
    {8, 8, 47, 1, 1},
    {8, 8, 48, 1, 2},
    // type 1：blk_wh <= 24 时 d >= 4 即 3
    {12, 12, 3, 1, 0},
    {12, 12, 4, 1, 3},
    // type 1：blk_wh > 24 时 d >= 1 即 3
    {16, 16, 0, 1, 0},
    {16, 16, 1, 1, 3},
};

struct UpsampleCase {
    int bs0;
    int bs1;
    int delta;
    int type;
    int expected;
};

constexpr UpsampleCase kUpsampleCases[] = {
    {8, 8, 0, 0, 0},    // d == 0 一律不上采样
    {8, 8, 1, 0, 1},    // 亮度块 blk_wh <= 16 且 0 < d < 40
    {8, 8, 39, 0, 1},
    {8, 8, 40, 0, 0},   // d >= 40 一律不上采样
    {18, 18, 20, 0, 0},
    {4, 4, 20, 1, 1},   // type 1 只在 blk_wh <= 8
    {8, 8, 20, 1, 0},
};

}  // namespace

TEST_CASE("av1 intra edge filter strength: type 0/1 阈值表") {
    for (const auto& c : kStrengthCases) {
        INFO("bs0=" << c.bs0 << " bs1=" << c.bs1 << " delta=" << c.delta
                    << " type=" << c.type);
        CHECK(intra_edge_filter_strength(c.bs0, c.bs1, c.delta, c.type) ==
              c.expected);
    }
}

TEST_CASE("av1 intra edge: 亮度块邻居含 SMOOTH 时按 type 1 取强度") {
    // 8x8 亮度块、angle_delta 使 |p_angle-90| = 48：
    // type 0 只到 1，type 1 已到 2。这条正是 8x16 下块预测差 1..2 的来源。
    CHECK(intra_edge_filter_strength(8, 8, 48, 0) == 1);
    CHECK(intra_edge_filter_strength(8, 8, 48, 1) == 2);
}

TEST_CASE("av1 intra edge upsample: 触发条件") {
    for (const auto& c : kUpsampleCases) {
        INFO("bs0=" << c.bs0 << " bs1=" << c.bs1 << " delta=" << c.delta
                    << " type=" << c.type);
        CHECK(use_intra_edge_upsample(c.bs0, c.bs1, c.delta, c.type) ==
              c.expected);
    }
}

TEST_CASE("av1 intra edge filter: 三档核与 16 除数") {
    constexpr int kSz = 9;
    constexpr std::array<std::uint8_t, kSz> kSrc = {40, 80, 120, 160,
                                                   200, 60, 100, 140, 180};
    // 越界按端点复制（规范里的复制规则），返回值统一是 int 免符号告警。
    auto at = [&](int k) {
        const int c = k < 0 ? 0 : (k > kSz - 1 ? kSz - 1 : k);
        return static_cast<int>(kSrc[static_cast<std::size_t>(c)]);
    };

    // strength = 1：{0,4,8,4,0}/16 即 (a + 2*b + c + 2) >> 2。
    auto s1 = kSrc;
    filter_intra_edge(s1.data(), kSz, 1);
    CHECK(static_cast<int>(s1[0]) == at(0));  // 首样本不动
    for (int i = 1; i < kSz; ++i) {
        const int v = at(i - 1) + 2 * at(i) + at(i + 1);
        CHECK(static_cast<int>(s1[static_cast<std::size_t>(i)]) == ((v + 2) >> 2));
    }

    // strength = 2：{0,5,6,5,0}/16。
    auto s2 = kSrc;
    filter_intra_edge(s2.data(), kSz, 2);
    CHECK(static_cast<int>(s2[0]) == at(0));
    for (int i = 1; i < kSz; ++i) {
        const int v = 5 * at(i - 1) + 6 * at(i) + 5 * at(i + 1);
        CHECK(static_cast<int>(s2[static_cast<std::size_t>(i)]) == ((v + 8) >> 4));
    }

    // strength = 3：{2,4,4,4,2}/16。
    auto s3 = kSrc;
    filter_intra_edge(s3.data(), kSz, 3);
    CHECK(static_cast<int>(s3[0]) == at(0));
    for (int i = 1; i < kSz; ++i) {
        const int v = 2 * at(i - 2) + 4 * at(i - 1) + 4 * at(i) +
                      4 * at(i + 1) + 2 * at(i + 2);
        CHECK(static_cast<int>(s3[static_cast<std::size_t>(i)]) == ((v + 8) >> 4));
    }

    // strength = 0 不改动任何样本。
    auto s0 = kSrc;
    filter_intra_edge(s0.data(), kSz, 0);
    CHECK(s0 == kSrc);
}

TEST_CASE("av1 intra edge corner: 5/6/5 只改角点") {
    // above 的 -1 是角像素，left 的 0 是左侧第一个样本（写入落在 left[-1]）。
    std::uint8_t above[3] = {90, 100, 110};
    std::uint8_t left[3] = {0, 120, 130};
    const int expected = (120 * 5 + 90 * 6 + 100 * 5 + 8) >> 4;  // 103

    filter_intra_edge_corner(above + 1, left + 1);
    CHECK(static_cast<int>(above[0]) == expected);   // above[-1]
    CHECK(static_cast<int>(left[0]) == expected);    // left[-1]
    CHECK(above[1] == 100);  // 其余样本不受影响
    CHECK(left[1] == 120);
}

TEST_CASE("av1 intra edge upsample: 半样本 9/9/-1/-1 插值") {
    // p[-1]=100，p[0..3]=110/120/130/140。
    std::array<std::uint8_t, 12> buf{};
    std::uint8_t* p = buf.data() + 2;
    p[-1] = 100;
    p[0] = 110;
    p[1] = 120;
    p[2] = 130;
    p[3] = 140;

    upsample_intra_edge(p, 4);

    CHECK(p[-2] == 100);  // 规范：p[-2] 直接取 p[-1]
    CHECK(p[0] == 110);
    CHECK(p[2] == 120);
    CHECK(p[4] == 130);
    CHECK(p[6] == 140);
    // 半样本：Round2(-in[i] + 9*in[i+1] + 9*in[i+2] - in[i+3], 4)，
    // in = {p[-1], p[-1], p[0..]}。
    constexpr int in[7] = {100, 100, 110, 120, 130, 140, 140};
    for (int i = 0; i < 4; ++i) {
        const int v = -in[i] + 9 * in[i + 1] + 9 * in[i + 2] - in[i + 3];
        CHECK(static_cast<int>(p[2 * i - 1]) == ((v + 8) >> 4));
    }
}
