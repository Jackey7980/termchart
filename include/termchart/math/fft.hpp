// math/fft.hpp —— 快速傅里叶变换（header-only）
//
// Radix-2 Cooley-Tukey，原地计算（除 twiddle 外零分配），re/im 双 span 布局
// （复数交错由调用方决定，原语本身不替谁做决定）。
// 设计约定：
//   * n 必须是 2 的幂且 re/im 等长——否则返回 false 且**不改数据**
//     （fail-loud：让调用方去 pad，不悄悄挑个邻近长度糊弄）。
//   * 逆变换缩放 1/N，保证 forward → inverse 往返恒等（与 scipy 口径一致）。
//   * twiddle 每个 stage 直接算 cos/sin 不用递推——递推的误差随 log N 累积，
//     可视化频谱不需要为省几次 trig 赔上精度。
//   * 不抛异常；span 为空/不等长/非 2 幂均安全返回。
#ifndef TERMCHART_MATH_FFT_HPP
#define TERMCHART_MATH_FFT_HPP

#include <cmath>
#include <cstddef>
#include <span>

namespace termchart::math {

// 不小于 n 的最小 2 的幂（n == 0 时返回 1）。
[[nodiscard]] constexpr std::size_t next_pow2(std::size_t n) noexcept {
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

// 原地 radix-2 FFT。re/im 长度相等且为 2 的幂；inverse = true 时做逆变换并缩放 1/N。
// 成功返回 true；输入不合法返回 false（数据保持原样）。
[[nodiscard]] inline bool fft(std::span<double> re, std::span<double> im, bool inverse = false) noexcept {
    if (re.size() != im.size() || re.empty()) return false;
    std::size_t n = re.size();
    if ((n & (n - 1)) != 0) return false;  // 非 2 的幂

    // 位反转置换（原地交换）
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            const double tr = re[i];
            re[i] = re[j];
            re[j] = tr;
            const double ti = im[i];
            im[i] = im[j];
            im[j] = ti;
        }
    }

    // 逐级蝶形：stage 长度 2, 4, 8, ...
    const double sign = inverse ? 1.0 : -1.0;
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const double ang = sign * 2.0 * 3.14159265358979323846 / static_cast<double>(len);
        const std::size_t half = len >> 1;
        for (std::size_t i = 0; i < n; i += len) {
            for (std::size_t k = 0; k < half; ++k) {
                const double wr = std::cos(ang * static_cast<double>(k));
                const double wi = std::sin(ang * static_cast<double>(k));
                const std::size_t a = i + k;
                const std::size_t b = a + half;
                const double tr = re[b] * wr - im[b] * wi;
                const double ti = re[b] * wi + im[b] * wr;
                re[b] = re[a] - tr;
                im[b] = im[a] - ti;
                re[a] += tr;
                im[a] += ti;
            }
        }
    }

    if (inverse) {
        const double s = 1.0 / static_cast<double>(n);
        for (std::size_t i = 0; i < n; ++i) {
            re[i] *= s;
            im[i] *= s;
        }
    }
    return true;
}

}  // namespace termchart::math

#endif  // TERMCHART_MATH_FFT_HPP
