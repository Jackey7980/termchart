// random.hpp —— 可复现伪随机（header-only）
//
// 为什么自带：**可复现**是硬需求。散点抖动、力导向初始布局、用例 fixture
// 都要"同一个种子得到同一个结果"，std::random_device/mt19937 的默认初始化做不到。
// 算法：splitmix64（无状态争用、质量足够、实现 20 行）。
#ifndef TERMCHART_UTIL_RANDOM_HPP
#define TERMCHART_UTIL_RANDOM_HPP

#include <cstdint>
#include <limits>

namespace termchart::util {

class Rng {
public:
    using result_type = std::uint64_t;

    constexpr explicit Rng(std::uint64_t seed = 0x9E3779B97F4A7C15ull) noexcept : state_(seed) {}

    constexpr std::uint64_t next() noexcept {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    constexpr double unit() noexcept {  // [0, 1)
        return static_cast<double>(next() >> 11) * (1.0 / (1ull << 53));
    }

    constexpr double range(double lo, double hi) noexcept { return lo + (hi - lo) * unit(); }

    constexpr std::uint64_t integer(std::uint64_t bound) noexcept {
        return bound == 0 ? 0 : next() % bound;
    }

    static constexpr result_type min() noexcept { return 0; }
    static constexpr result_type max() noexcept { return std::numeric_limits<result_type>::max(); }

private:
    std::uint64_t state_;
};

}  // namespace termchart::util

#endif  // TERMCHART_UTIL_RANDOM_HPP
