// colormap.hpp —— 连续色表：viridis / inferno / magma / plasma / turbo
//
// 为什么用**多项式拟合**而不是数据文件：
//   * 避免把原始数据（CC0）打包进仓库，也避免需要在运行期加载 asset；
//   * 拟合的最大误差约 2/255，终端着色场景不可分辨；
//   * 于是 colormap 是纯函数，无状态、线程安全、无需初始化。
// 拟合系数来自公开的多项式拟合（Google, "Analytic Approximations for Colormaps"），
// 见 docs/reference/module_matrix.md 之外的说明；需要权威数据时用 utile/sample 表替换。
#ifndef TERMCHART_COLOR_COLORMAP_HPP
#define TERMCHART_COLOR_COLORMAP_HPP

#include <cstddef>
#include <cstdint>
#include <span>

#include "termchart/core/rgb.hpp"
#include "termchart/export.hpp"

namespace termchart::color {

enum class ColormapKind : std::uint8_t {
    Viridis = 0,
    Inferno = 1,
    Magma = 2,
    Plasma = 3,
    Turbo = 4,
};

inline constexpr int kColormapCount = 5;

// t 会被钳到 [0,1]。真正的可见性/感知均匀性由色表本身保证，这里不插值采样不足。
TERMCHART_API Rgb sample(ColormapKind kind, double t) noexcept;

// 预采样成均匀色带（默认 256 级），缓存于进程内：热力图/色标每秒查几万次，
// 每次跑 7 次多项式太贵。首次调用构建，之后只读——**无锁**：单线程主循环前提成立。
TERMCHART_API std::span<const Rgb> stops(ColormapKind kind, std::size_t count = 256);

TERMCHART_API const char* colormap_name(ColormapKind kind) noexcept;

}  // namespace termchart::color

#endif  // TERMCHART_COLOR_COLORMAP_HPP
