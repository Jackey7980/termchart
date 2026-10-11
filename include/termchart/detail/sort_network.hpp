// sort_network.hpp —— 小数组排序网络（层号 -1 横切数据表）
//
// 为什么用排序网络：glibc 的 std::sort 对小 n 也有调用开销，而 layout/label_avoid、
// raster 子像素合成里存在大量"固定长度 2~8 的数组、每帧排序"的热点。
// 排序网络无分支预测失败、无递归，且 constexpr 可在编译期跑完。
//
// 网络本体是"比较器下标对"表：pair {i, j} 表示无条件交换 a[i] 与 a[j]（若逆序）。
#ifndef TERMCHART_DETAIL_SORT_NETWORK_HPP
#define TERMCHART_DETAIL_SORT_NETWORK_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace termchart::detail {

struct ComparePair {
    std::uint8_t i;
    std::uint8_t j;
};

// ── n = 2..8 的最优/准最优网络（下标对，按执行顺序排列） ─────────────────
using Network2 = std::array<ComparePair, 1>;
using Network3 = std::array<ComparePair, 3>;
using Network4 = std::array<ComparePair, 5>;
using Network5 = std::array<ComparePair, 9>;
using Network6 = std::array<ComparePair, 12>;
using Network7 = std::array<ComparePair, 16>;
using Network8 = std::array<ComparePair, 19>;

// clang-format off
inline constexpr Network2 kSortNetwork2{{{0, 1}}};
inline constexpr Network3 kSortNetwork3{{{0, 1}, {0, 2}, {1, 2}}};
inline constexpr Network4 kSortNetwork4{{{0, 1}, {2, 3}, {0, 2}, {1, 3}, {1, 2}}};
inline constexpr Network5 kSortNetwork5{{
    {0, 1}, {2, 3}, {1, 3}, {2, 4}, {0, 2}, {1, 4}, {1, 2}, {3, 4}, {2, 3},
}};
inline constexpr Network6 kSortNetwork6{{
    {0, 1}, {2, 3}, {4, 5}, {0, 2}, {1, 4}, {3, 5}, {0, 1}, {2, 3},
    {4, 5}, {1, 2}, {3, 4}, {2, 3},
}};
inline constexpr Network7 kSortNetwork7{{
    {0, 1}, {2, 3}, {4, 5}, {0, 2}, {1, 3}, {4, 6}, {0, 4}, {1, 5},
    {2, 6}, {1, 2}, {3, 5}, {0, 1}, {2, 3}, {4, 5}, {3, 4}, {5, 6},
}};
inline constexpr Network8 kSortNetwork8{{
    {0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7},
    {1, 2}, {5, 6}, {0, 4}, {3, 7}, {1, 5}, {2, 6}, {1, 4}, {3, 6},
    {2, 4}, {3, 5}, {3, 4},
}};
// clang-format on

// 按网络执行 compare-exchange。Comp：严格弱序，返回 true 表示 a 应排在 b 前。
template <class T, std::size_t N, std::size_t M, class Comp>
constexpr void apply_network(std::array<T, N>& a, const std::array<ComparePair, M>& net,
                             Comp comp) {
    static_assert(N >= 2 && N <= 8, "sort_network 只覆盖 n ∈ [2, 8]");
    for (const ComparePair& p : net) {
        T& x = a[p.i];
        T& y = a[p.j];
        if (comp(y, x)) {
            T tmp = std::move(x);
            x = std::move(y);
            y = std::move(tmp);
        }
    }
}

}  // namespace termchart::detail

#endif  // TERMCHART_DETAIL_SORT_NETWORK_HPP
