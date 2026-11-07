// transform.cpp —— 4x4 整数 DCT 蝶形（H.264/HEVC 同构核心变换）
#include "transform.hpp"

namespace termchart::image::codec::hevc {

void fwd_transform_4x4(const std::int16_t* residual, std::int32_t* coeff) {
    std::int32_t t[16];
    for (int i = 0; i < 4; ++i) {
        const std::int16_t* r = residual + i * 4;
        const std::int32_t s0 = r[0] + r[3], s1 = r[1] + r[2];
        const std::int32_t s2 = r[1] - r[2], s3 = r[0] - r[3];
        t[i * 4 + 0] = s0 + s1;
        t[i * 4 + 1] = s2 + (s3 << 1);
        t[i * 4 + 2] = s0 - s1;
        t[i * 4 + 3] = s3 - (s2 << 1);
    }
    for (int j = 0; j < 4; ++j) {
        const std::int32_t d0 = t[j], d1 = t[4 + j], d2 = t[8 + j], d3 = t[12 + j];
        const std::int32_t s0 = d0 + d3, s1 = d1 + d2;
        const std::int32_t s2 = d1 - d2, s3 = d0 - d3;
        coeff[j] = s0 + s1;
        coeff[4 + j] = s2 + (s3 << 1);
        coeff[8 + j] = s0 - s1;
        coeff[12 + j] = s3 - (s2 << 1);
    }
}

void inv_transform_4x4(const std::int32_t* in, std::int16_t* residual) {
    std::int32_t t[16];
    for (int i = 0; i < 4; ++i) {
        const std::int32_t d0 = in[i * 4 + 0], d1 = in[i * 4 + 1];
        const std::int32_t d2 = in[i * 4 + 2], d3 = in[i * 4 + 3];
        const std::int32_t e0 = d0 + d2, e1 = d0 - d2;
        const std::int32_t e2 = (d1 >> 1) - d3, e3 = d1 + (d3 >> 1);
        t[i * 4 + 0] = e0 + e3;
        t[i * 4 + 1] = e1 + e2;
        t[i * 4 + 2] = e1 - e2;
        t[i * 4 + 3] = e0 - e3;
    }
    for (int j = 0; j < 4; ++j) {
        const std::int32_t d0 = t[j], d1 = t[4 + j], d2 = t[8 + j], d3 = t[12 + j];
        const std::int32_t e0 = d0 + d2, e1 = d0 - d2;
        const std::int32_t e2 = (d1 >> 1) - d3, e3 = d1 + (d3 >> 1);
        residual[j] = static_cast<std::int16_t>((e0 + e3 + 32) >> 6);
        residual[4 + j] = static_cast<std::int16_t>((e1 + e2 + 32) >> 6);
        residual[8 + j] = static_cast<std::int16_t>((e1 - e2 + 32) >> 6);
        residual[12 + j] = static_cast<std::int16_t>((e0 - e3 + 32) >> 6);
    }
}

}  // namespace termchart::image::codec::hevc
