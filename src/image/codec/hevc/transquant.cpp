// transquant.cpp —— 残差量化与重建（4/8/16/32 + transform_skip）
#include "transquant.hpp"

#include <algorithm>

#include "quantize.hpp"
#include "transform.hpp"

namespace termchart::image::codec::hevc {
namespace {

inline std::uint8_t clamp8(int v) {
    return static_cast<std::uint8_t>(std::clamp(v, 0, 255));
}

void build_residual(const std::uint8_t* src, int src_stride, const std::uint8_t* pred,
                    int n, std::int16_t* residual) {
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            residual[y * n + x] = static_cast<std::int16_t>(
                static_cast<int>(src[y * src_stride + x]) - pred[y * n + x]);
}

void add_residual(const std::int16_t* residual, const std::uint8_t* pred, int n,
                  std::uint8_t* rec, int rec_stride) {
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            rec[y * rec_stride + x] = clamp8(
                static_cast<int>(pred[y * n + x]) + residual[y * n + x]);
}

}  // namespace

void reconstruct_tu(const std::int16_t* level, int qp, const std::uint8_t* pred,
                   std::uint8_t* rec, int rec_stride, int log2_size, bool use_dst,
                   bool transform_skip) {
    const int n = 1 << log2_size;
    std::int32_t dq[32 * 32];
    std::int16_t residual[32 * 32];
    dequant(level, dq, qp, log2_size, transform_skip);
    if (transform_skip) {
        const int cnt = n * n;
        for (int i = 0; i < cnt; ++i)
            residual[i] = static_cast<std::int16_t>(
                std::clamp<std::int64_t>(dq[i], -32768, 32767));
    } else {
        inv_transform(dq, residual, log2_size, use_dst, n);
    }
    add_residual(residual, pred, n, rec, rec_stride);
}

int encode_tu(const std::uint8_t* src, int src_stride, const std::uint8_t* pred,
              std::uint8_t* rec, int rec_stride, std::int16_t* level, int qp,
              int log2_size, bool use_dst, bool transform_skip) {
    const int n = 1 << log2_size;
    std::int16_t residual[32 * 32];
    build_residual(src, src_stride, pred, n, residual);

    std::int32_t coeff[32 * 32];
    if (transform_skip) {
        const int cnt = n * n;
        for (int i = 0; i < cnt; ++i) coeff[i] = static_cast<std::int32_t>(residual[i]);
    } else {
        fwd_transform(residual, coeff, log2_size, use_dst);
    }
    const int nz = quant(coeff, level, qp, log2_size, transform_skip);
    // 重建必须用量化后的 level，否则编码端预测基准与解码端不一致。
    reconstruct_tu(level, qp, pred, rec, rec_stride, log2_size, use_dst, transform_skip);
    return nz;
}

int encode_tu_4x4(const std::uint8_t* src, int src_stride, const std::uint8_t* pred,
                  std::uint8_t* rec, int rec_stride, std::int16_t* level, int qp,
                  bool use_dst) {
    return encode_tu(src, src_stride, pred, rec, rec_stride, level, qp, 2, use_dst,
                     false);
}

void reconstruct_tu_4x4(const std::int16_t* level, int qp, const std::uint8_t* pred,
                        std::uint8_t* rec, int rec_stride, bool use_dst) {
    reconstruct_tu(level, qp, pred, rec, rec_stride, 2, use_dst, false);
}

}  // namespace termchart::image::codec::hevc
