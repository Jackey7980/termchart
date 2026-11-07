// transquant.cpp —— 4x4 TU 的残差量化与重建
#include "transquant.hpp"

#include <algorithm>

#include "quantize.hpp"
#include "transform.hpp"

namespace termchart::image::codec::hevc {
namespace {

inline std::uint8_t clamp8(int v) {
    return static_cast<std::uint8_t>(std::clamp(v, 0, 255));
}

}  // namespace

void reconstruct_tu_4x4(const std::int16_t* level, int qp,
                        const std::uint8_t* pred, std::uint8_t* rec,
                        int rec_stride) {
    std::int32_t dq[16];
    std::int16_t residual[16];
    dequant_4x4(level, dq, qp);
    inv_transform_4x4(dq, residual);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            rec[y * rec_stride + x] = clamp8(static_cast<int>(pred[y * 4 + x]) +
                                             residual[y * 4 + x]);
}

int encode_tu_4x4(const std::uint8_t* src, int src_stride,
                  const std::uint8_t* pred, std::uint8_t* rec, int rec_stride,
                  std::int16_t* level, int qp) {
    std::int16_t residual[16];
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            residual[y * 4 + x] = static_cast<std::int16_t>(
                static_cast<int>(src[y * src_stride + x]) - pred[y * 4 + x]);

    std::int32_t coeff[16];
    fwd_transform_4x4(residual, coeff);
    const int nz = quant_4x4(coeff, level, qp);
    // 重建必须用量化后的 level，否则编码端预测基准与解码端不一致。
    reconstruct_tu_4x4(level, qp, pred, rec, rec_stride);
    return nz;
}

}  // namespace termchart::image::codec::hevc
