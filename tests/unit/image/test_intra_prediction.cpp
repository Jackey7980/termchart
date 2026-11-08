// test_intra_prediction.cpp —— HEVC 4x4 intra DC 预测语义回归测试
//
// oracle 来自真实 ffmpeg 解码像素反推（_cu_grid_test.cpp / t1 形态）：
// CU1 TU0 的 left 参考 = [144,141,137,142]、top 不可用（帧顶），ffmpeg 解码出的
// 预测块（残差为 0 的等效反推）为
//     144 143 143 143
//     143 143 143 143
//     142 143 143 143
//     143 143 143 143
// 即 HEVC 8.4.4.2.2 参考样本替代 + 8.4.4.2.5 DC 边界滤波。
//
// 早期实现把 DC 预测写成「整块填单一 dcVal」，在常数图上与 ffmpeg 一致
// （flat128 逐位相等），但在非平坦参考上产生系统性偏差并沿重建预测链传播，
// 导致 enc-recon 与 ffmpeg 解码不一致。此测试锁住修正后的语义。
#include <doctest.h>

#include <array>
#include <cstdint>

#include "src/image/codec/hevc/intra_prediction.hpp"

namespace {

using namespace termchart::image::codec::hevc;

std::array<std::uint8_t, 16> predict(const std::array<std::uint8_t, 5>& top,
                                    const std::array<std::uint8_t, 5>& left,
                                    bool has_top, bool has_left, int mode = 1) {
    std::array<std::uint8_t, 16> dst{};
    predict_intra(mode, 4, top.data(), left.data(), has_top, has_left, dst.data());
    return dst;
}

}  // namespace

TEST_CASE("intra DC: constant neighbours are a fixed point (flat128 regression)") {
    const std::array<std::uint8_t, 5> all128 = {128, 128, 128, 128, 128};
    const auto p = predict(all128, all128, true, true);
    for (int i = 0; i < 16; ++i) CHECK(p[i] == 128);

    // 两侧都不可用：规范用 1<<(bitDepth-1)=128 全填充，边界滤波仍恒等。
    const std::array<std::uint8_t, 5> zero = {0, 0, 0, 0, 0};
    const auto q = predict(zero, zero, false, false);
    for (int i = 0; i < 16; ++i) CHECK(q[i] == 128);
}

TEST_CASE("intra DC: top unavailable substitutes left[0] and applies boundary filtering") {
    // FFmpeg oracle（t1 形态 CU1 TU0）。top 侧不可用，调用方传入的 top 内容
    // 不应参与计算；正确实现必须用 left[0] 替代整条 top 行。
    const std::array<std::uint8_t, 5> top = {0, 0, 0, 0, 0};
    const std::array<std::uint8_t, 5> left = {144, 141, 137, 142, 0};
    const auto p = predict(top, left, false, true);

    const std::array<std::uint8_t, 16> expected = {
        144, 143, 143, 143,
        143, 143, 143, 143,
        142, 143, 143, 143,
        143, 143, 143, 143,
    };
    for (int i = 0; i < 16; ++i) CHECK(static_cast<int>(p[i]) == static_cast<int>(expected[i]));
}

TEST_CASE("intra DC: dcVal uses both sides and boundary filter uses 3*dc+2") {
    // 双侧可用、非平坦：dcVal = (Σtop + Σleft + 4) >> 3。
    const std::array<std::uint8_t, 5> top = {100, 110, 120, 130, 0};
    const std::array<std::uint8_t, 5> left = {140, 150, 160, 170, 0};
    const auto p = predict(top, left, true, true);

    const int sum_top = 100 + 110 + 120 + 130;   // 460
    const int sum_left = 140 + 150 + 160 + 170;  // 620
    const int dc = (sum_top + sum_left + 4) >> 3;  // 135
    CHECK(dc == 135);

    CHECK(static_cast<int>(p[0]) == ((left[0] + 2 * dc + top[0] + 2) >> 2));
    for (int x = 1; x < 4; ++x)
        CHECK(static_cast<int>(p[x]) == ((top[x] + 3 * dc + 2) >> 2));
    for (int y = 1; y < 4; ++y)
        CHECK(static_cast<int>(p[y * 4]) == ((left[y] + 3 * dc + 2) >> 2));
    for (int y = 1; y < 4; ++y)
        for (int x = 1; x < 4; ++x) CHECK(static_cast<int>(p[y * 4 + x]) == dc);
}

TEST_CASE("intra DC: left unavailable substitutes top[0]") {
    const std::array<std::uint8_t, 5> top = {90, 100, 110, 120, 0};
    const std::array<std::uint8_t, 5> left = {0, 0, 0, 0, 0};
    const auto p = predict(top, left, true, false);

    const int sum = (90 + 100 + 110 + 120) + 4 * 90;
    const int dc = (sum + 4) >> 3;
    CHECK(static_cast<int>(p[0]) == ((90 + 2 * dc + top[0] + 2) >> 2));
    for (int x = 1; x < 4; ++x)
        CHECK(static_cast<int>(p[x]) == ((top[x] + 3 * dc + 2) >> 2));
    for (int y = 1; y < 4; ++y)
        CHECK(static_cast<int>(p[y * 4]) == ((90 + 3 * dc + 2) >> 2));
    for (int y = 1; y < 4; ++y)
        for (int x = 1; x < 4; ++x) CHECK(static_cast<int>(p[y * 4 + x]) == dc);
}
