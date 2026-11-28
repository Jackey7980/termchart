// metrics.hpp —— 无状态失真度量原语（累加器类型由调用方决定）。
//
// AV1 与 HEVC 的内核循环在此统一，精度/溢出口径仍由调用方选 AccumType：
// AV1 走 uint64_t，HEVC 的 4K 块走 long long。侧 stride 必须可分别指定——
// 源图与重建缓冲的行距本来就不同，单 stride 模板套不进真实调用点。
// 语义只含 diff 与累加，不夹带任何 band / RD 统计。
#ifndef TERMCHART_SRC_MEDIA_ENCODER_GPA_METRICS_HPP
#define TERMCHART_SRC_MEDIA_ENCODER_GPA_METRICS_HPP

#include <cstddef>
#include <cstdint>

namespace termchart::media::encoder::gpa {

// 扁平平面 SSD：8-bit 输入，平方误差和，不做归一化。
template <typename AccumType>
[[nodiscard]] inline AccumType calc_ssd(const std::uint8_t* a, const std::uint8_t* b,
                                        std::size_t n) noexcept {
    AccumType sum = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const int d = static_cast<int>(a[i]) - static_cast<int>(b[i]);
        sum += static_cast<AccumType>(d * d);
    }
    return sum;
}

// 二维平面 SSD：a / b 可有不同的行距；语义与逐像素 d*d 累加完全一致。
template <typename AccumType>
[[nodiscard]] inline AccumType calc_ssd(const std::uint8_t* a, int a_stride,
                                        const std::uint8_t* b, int b_stride,
                                        int width, int height) noexcept {
    AccumType sum = 0;
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* ra = a + static_cast<std::size_t>(y) * static_cast<std::size_t>(a_stride);
        const std::uint8_t* rb = b + static_cast<std::size_t>(y) * static_cast<std::size_t>(b_stride);
        for (int x = 0; x < width; ++x) {
            const int d = static_cast<int>(ra[x]) - static_cast<int>(rb[x]);
            sum += static_cast<AccumType>(d * d);
        }
    }
    return sum;
}

// 扁平平面 SAD：作为基础原语提供；当前编码器里没有纯 SAD 累加调用点。
template <typename AccumType>
[[nodiscard]] inline AccumType calc_sad(const std::uint8_t* a, const std::uint8_t* b,
                                        std::size_t n) noexcept {
    AccumType sum = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const int d = static_cast<int>(a[i]) - static_cast<int>(b[i]);
        sum += static_cast<AccumType>(d < 0 ? -d : d);
    }
    return sum;
}

template <typename AccumType>
[[nodiscard]] inline AccumType calc_sad(const std::uint8_t* a, int a_stride,
                                        const std::uint8_t* b, int b_stride,
                                        int width, int height) noexcept {
    AccumType sum = 0;
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* ra = a + static_cast<std::size_t>(y) * static_cast<std::size_t>(a_stride);
        const std::uint8_t* rb = b + static_cast<std::size_t>(y) * static_cast<std::size_t>(b_stride);
        for (int x = 0; x < width; ++x) {
            const int d = static_cast<int>(ra[x]) - static_cast<int>(rb[x]);
            sum += static_cast<AccumType>(d < 0 ? -d : d);
        }
    }
    return sum;
}

// 兼容既有 AV1 入口的扁平别名，返回 uint64_t。
[[nodiscard]] inline std::uint64_t ssd(const std::uint8_t* a, const std::uint8_t* b,
                                       std::size_t n) noexcept {
    return calc_ssd<std::uint64_t>(a, b, n);
}

}  // namespace termchart::media::encoder::gpa

#endif  // TERMCHART_SRC_MEDIA_ENCODER_GPA_METRICS_HPP
