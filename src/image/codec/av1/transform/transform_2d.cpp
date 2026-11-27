#include "transform_2d.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "qmatrix_data.hpp"

namespace termchart::image::codec::av1 {
namespace {

constexpr double kPi = 3.14159265358979323846;

// 1D 正交基，定标 256。DCT 的 DC 基与 AV1 规范 8 抽头定点核逐值一致
// （输入 256 时直流输出 181），因此它同时兼容仓内既有的 bit-exact IDCT8 路径。
double basis_value(int n, int k, int i, Av1Transform1d type) {
    switch (type) {
    case Av1Transform1d::Identity:
        return (k == i) ? 1.0 : 0.0;
    case Av1Transform1d::Adst:
    case Av1Transform1d::FlipAdst: {
        const int kk = type == Av1Transform1d::FlipAdst ? n - 1 - k : k;
        return std::sqrt(2.0 / (n + 1.0)) *
               std::sin(kPi * (i + 1) * (kk + 1) / (n + 1.0));
    }
    case Av1Transform1d::Dct:
    default:
        // sqrt(2/n) 的整体因子由反变换末尾的 >>4 承担；这里只留 DC 的 1/sqrt(2)。
        return (k == 0 ? 1.0 / std::sqrt(2.0) : 1.0) *
               std::cos(kPi * (2 * i + 1) * k / (2.0 * n));
    }
}

int round_i(double v) {
    return static_cast<int>(v < 0 ? std::ceil(v - 0.5) : std::floor(v + 0.5));
}

// 系数 -> 样本（B·c）。B·Bᵀ = 256²·I，所以正变换用 Bᵀ/256²。
void inverse_1d(const std::int16_t* in, std::int32_t* out, int n,
                Av1Transform1d type) {
    for (int i = 0; i < n; ++i) {
        double sum = 0;
        for (int k = 0; k < n; ++k)
            sum += basis_value(n, k, i, type) * in[k];
        out[i] = round_i(sum);
    }
}

// DCT 的 B·Bᵀ = (n/2)·I，ADST(DST-VII) / IDTX 的 B·Bᵀ = I。
double basis_energy(int n, Av1Transform1d type) {
    return type == Av1Transform1d::Dct ? n / 2.0 : 1.0;
}

void forward_1d(const std::int32_t* in, std::int16_t* out, int n,
                Av1Transform1d type) {
    const double denom = basis_energy(n, type);
    for (int k = 0; k < n; ++k) {
        double sum = 0;
        for (int i = 0; i < n; ++i)
            sum += basis_value(n, k, i, type) * in[i];
        out[k] = static_cast<std::int16_t>(round_i(sum / denom));
    }
}

void clip_time(std::int32_t* v, int n) {
    for (int i = 0; i < n; ++i) v[i] = std::clamp(v[i], -32768, 32767);
}

}  // namespace

void av1_inverse_transform_2d(const std::int16_t* coeff, std::int32_t* out,
                              int w, int h, Av1Transform1d tx,
                              Av1Transform1d ty) {
    std::int32_t tmp[64][64];
    std::int16_t row_in[64], col_in[64];
    std::int32_t row_out[64], col_out[64];
    w = std::clamp(w, 2, 64);
    h = std::clamp(h, 2, 64);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) row_in[x] = coeff[y * w + x];
        inverse_1d(row_in, row_out, w, tx);
        clip_time(row_out, w);
        for (int x = 0; x < w; ++x) tmp[y][x] = row_out[x];
    }
    // 与既有残差链一致：两遍之间右移 1 位，最后再右移 4 位。
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) col_in[y] =
            static_cast<std::int16_t>((tmp[y][x] + 1) >> 1);
        inverse_1d(col_in, col_out, h, ty);
        clip_time(col_out, h);
        for (int y = 0; y < h; ++y) out[y * w + x] = col_out[y];
    }
}

void av1_forward_transform_2d(const std::int32_t* residual, std::int16_t* coeff,
                              int w, int h, Av1Transform1d tx,
                              Av1Transform1d ty) {
    std::int32_t tmp[64][64];
    std::int16_t row_out[64], col_out[64];
    std::int32_t row_in[64], col_in[64];
    w = std::clamp(w, 2, 64);
    h = std::clamp(h, 2, 64);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) row_in[x] = residual[y * w + x] << 4;
        forward_1d(row_in, row_out, w, tx);
        for (int x = 0; x < w; ++x) tmp[y][x] = row_out[x] << 1;
    }
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) col_in[y] = tmp[y][x];
        forward_1d(col_in, col_out, h, ty);
        for (int y = 0; y < h; ++y) coeff[y * w + x] = col_out[y];
    }
}

void av1_scan_order(int w, int h, int scan_type, int* rc_out) {
    w = std::clamp(w, 2, 64);
    h = std::clamp(h, 2, 64);
    int n = 0;
    if (scan_type == 1) {  // HORZ：逐行，行内交替方向
        for (int y = 0; y < h; ++y) {
            for (int i = 0; i < w; ++i)
                rc_out[n++] = y * w + ((y & 1) ? w - 1 - i : i);
        }
    } else if (scan_type == 2) {  // VERT：逐列，列内交替方向
        for (int x = 0; x < w; ++x) {
            for (int i = 0; i < h; ++i)
                rc_out[n++] = ((x & 1) ? h - 1 - i : i) * w + x;
        }
    } else {  // ZIGZAG：反对角线，奇偶交替
        for (int s = 0; s < w + h - 1; ++s) {
            const int y0 = std::max(0, s - w + 1);
            const int y1 = std::min(h - 1, s);
            if (s & 1) {
                for (int y = y0; y <= y1; ++y) rc_out[n++] = y * w + (s - y);
            } else {
                for (int y = y1; y >= y0; --y) rc_out[n++] = y * w + (s - y);
            }
        }
    }
}

std::array<std::uint8_t, 64> av1_qmatrix_8x8(int level, bool chroma) {
    std::array<std::uint8_t, 64> qm{};
    if (level >= 15) {  // 不用 qmatrix：规范取常数 32
        qm.fill(32);
        return qm;
    }
    const int lv = std::clamp(level, 0, 14);
    const auto& src = kQMatrix8x8[lv][chroma ? 1 : 0];
    for (int i = 0; i < 64; ++i)
        qm[static_cast<std::size_t>(i)] = src[static_cast<std::size_t>(i)];
    return qm;
}

}  // namespace termchart::image::codec::av1
