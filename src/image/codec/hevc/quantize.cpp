// quantize.cpp —— 4x4 死区量化与逆量化
#include "quantize.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace termchart::image::codec::hevc {
namespace {

// 与 transform 的 4x4 核心变换配对的标准 MF / LevelScale。
// 列：0=偶偶 (0,0)(0,2)(2,0)(2,2)，1=混合，2=奇奇。
constexpr std::uint16_t kMf[6][3] = {
    {13107, 8066, 5243}, {11916, 7490, 4660}, {10082, 6554, 4194},
    {9362, 5825, 3647},  {8192, 5243, 3355},  {7282, 4559, 2893},
};
constexpr std::uint16_t kLevelScale[6][3] = {
    {10, 13, 16}, {11, 14, 18}, {13, 16, 20}, {14, 18, 23}, {16, 20, 25}, {18, 23, 29},
};

int pos_class(int i, int j) {
    const int a = i & 1, b = j & 1;
    if (a == 0 && b == 0) return 0;
    if (a == 1 && b == 1) return 2;
    return 1;
}

}  // namespace

int quant_4x4(const std::int32_t* coeff, std::int16_t* level, int qp) {
    qp = std::clamp(qp, 0, 51);
    const int qbits = 15 + qp / 6;
    const int f = static_cast<int>((static_cast<std::int64_t>(1) << qbits) / 3);
    const int row = qp % 6;
    int nonzero = 0;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            const int k = i * 4 + j;
            const std::int32_t mf = kMf[row][pos_class(i, j)];
            const std::int64_t a = std::abs(static_cast<std::int64_t>(coeff[k]));
            const std::int64_t v = (a * mf + f) >> qbits;
            level[k] = static_cast<std::int16_t>(coeff[k] < 0 ? -v : v);
            nonzero += (v != 0) ? 1 : 0;
        }
    }
    return nonzero;
}

void dequant_4x4(const std::int16_t* level, std::int32_t* coeff, int qp) {
    qp = std::clamp(qp, 0, 51);
    const int shift = qp / 6;
    const int row = qp % 6;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            const int k = i * 4 + j;
            const std::int32_t ls = kLevelScale[row][pos_class(i, j)];
            coeff[k] = static_cast<std::int32_t>(level[k]) * ls * (1 << shift);
        }
    }
}

double quant_step(int qp) {
    // 标准 Qstep(qp) = Qstep[qp%6] * 2^(qp/6)，随 QP 严格单调递增。
    static constexpr double kQstep[6] = {0.625, 0.6875, 0.8125, 0.875, 1.0, 1.125};
    qp = std::clamp(qp, 0, 51);
    return kQstep[qp % 6] * std::ldexp(1.0, qp / 6);
}

}  // namespace termchart::image::codec::hevc
