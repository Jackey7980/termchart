#include "termchart/color/colormap.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>
#include <vector>

#include "termchart/detail/turbo_table.hpp"

namespace termchart::color {
namespace {

struct Fit {
    double c[7][3];
};

// 六次多项式拟合系数（公开拟合结果，误差约 2/255）。
constexpr Fit kFits[kColormapCount] = {
    // Viridis
    {{{0.2777273272234177, 0.005407344544966578, 0.3340998053353061},
      {0.1050930431085774, 1.404613529898575, 1.384590162594685},
      {-0.3308618287255563, 0.214847559468213, 0.09509516302823659},
      {-4.634230498983486, -5.799100973351585, -19.33244095627987},
      {6.228269936347081, 14.17993336680509, 56.69055260068105},
      {4.776384997670288, -13.74514537774601, -65.35303263337226},
      {-5.435455855934631, 4.645852610178815, 26.3124352495832}}},
    // Inferno
    {{{0.0002189403691192265, 0.001651004631001014, -0.01948089843709181},
      {0.1065134194856116, 0.5639564367884091, 3.932712388889277},
      {11.60249308247187, -3.972853965665698, -15.9423941062914},
      {-41.70399613139459, 17.43639888205313, 44.35414519872813},
      {77.162935699427, -33.40235894210092, -81.80730925738993},
      {-71.31942824499214, 32.62606426397723, 73.20951985803202},
      {25.13112622477341, -12.24266895238567, -23.07032500287172}}},
    // Magma
    {{{-0.002136485053939682, -0.000749655052795221, -0.005386127855323933},
      {0.2516605407371642, 0.6775232436837668, 2.494026599312351},
      {8.353717279216625, -3.577719514958484, 0.3144679030132573},
      {-27.668393816218659, 14.264530442161652, 13.649213437589398},
      {52.17613981234068, -27.9436061026845, -12.944169442589924},
      {-50.33226534031434, 29.46097861836572, 4.234081987718046},
      {18.19416994836131, -11.022622191210948, -0.6235273400251491}}},
    // Plasma
    {{{0.05873234392399702, 0.02333670892565664, 0.5433401826748754},
      {2.176514634195958, 0.2383834171260182, 0.7539604590484035},
      {-2.689460476458034, 7.455851135738909, 3.110799939717086},
      {6.130348345893603, -15.05784498389334, -12.32467536117919},
      {-6.998913856470392, 15.76347713715295, 21.38077904843575},
      {4.463786751456037, -8.774061486513235, -15.79756544919782},
      {-1.019267533871308, 2.087819150314942, 4.447397585685024}}},
    // Turbo（已废弃：系数损坏，现改走 kTurboStops 插值，见下方 turbo_sample）
    {{{0.1140890109226559, 0.06288340699912215, 0.2248337216805064},
      {6.716419497985708, 3.182286745507602, 7.571583586145393},
      {-66.09402360453038, -4.9279827041226, 10.09439367561635},
      {228.7660791526501, 25.04986699771073, -91.54105330182436},
      {-334.8351565777451, -69.31749712757485, 288.5858850615712},
      {218.7637218434795, 67.52150567819112, -305.2045772184957},
      {-52.88903478218835, -21.54527364654712, 110.5174647748972}}},
};

struct StopCache {
    std::mutex mutex;
    std::array<std::vector<Rgb>, kColormapCount> table{};
    std::array<std::size_t, kColormapCount> sizes{};

    std::vector<Rgb>& ensure(ColormapKind kind, std::size_t count) {
        // 用 size_t 而非 int 做下标：std::array 的下标是 size_t，int 会被
        // -Wsign-conversion 报出来（本文件历史上就带着这几条警告）。
        const std::size_t slot = static_cast<std::size_t>(kind);
        std::lock_guard<std::mutex> lock(mutex);
        if (sizes[slot] != count) {
            std::vector<Rgb> built;
            built.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                const double t = count <= 1 ? 0.0 : static_cast<double>(i) / static_cast<double>(count - 1);
                built.push_back(sample(kind, t));
            }
            table[slot] = std::move(built);
            sizes[slot] = count;
        }
        return table[slot];
    }
};

StopCache& cache() {
    static StopCache instance;
    return instance;
}

std::uint8_t clamp_byte(double v) noexcept {
    const double clamped = v < 0.0 ? 0.0 : (v > 255.0 ? 255.0 : v);
    return static_cast<std::uint8_t>(std::lround(clamped));
}

}  // namespace

// Turbo：**官方 256 级色表**（原始配色由 Anton Mikhailov / Google LLC 发布，
// Apache License 2.0，见 LICENSES/Apache-2.0.txt）。
//
// 这里不做任何"近似"：Turbo 本身没有解析式定义，Google 发布的就是一张 256 级查找表。
// 本项目的历史实现先是用了坏掉的多项式系数（输出恒为蓝/青/紫），后来改成锚点插值
// （形状像、但颜色与官方表不符）。现在的唯一真值是 assets/colormaps/turbo.dat，
// 由 tools/gen_turbo_table.py 生成 detail/turbo_table.hpp，本文件只做
// "查表 + 相邻级线性插值"——级点处与官方表逐字节一致，级间也不过是两级的线性过渡。
namespace {

Rgb turbo_sample(double x) noexcept {
    constexpr std::size_t n = detail::kTurboTable.size();
    const double scaled = x * static_cast<double>(n - 1);

    // 级点必须**精确命中**该级：i/255 在二进制浮点下不精确，直接向下取整会让
    // 恰好位于级点上的采样落到前一级（例如 128/255*255 = 127.999…→127），
    // 于是"取官方表第 i 项"这类不变量会悄悄偏一级。这里对浮点误差做一次吸附。
    const double nearest = std::round(scaled);
    if (std::fabs(scaled - nearest) < 1e-9 && nearest >= 0.0 &&
        nearest < static_cast<double>(n)) {
        const detail::TurboEntry& row = detail::kTurboTable[static_cast<std::size_t>(nearest)];
        return Rgb{row.r, row.g, row.b};
    }

    const auto index = static_cast<std::size_t>(scaled);
    if (index + 1 >= n) {
        const detail::TurboEntry& last = detail::kTurboTable[n - 1];
        return Rgb{last.r, last.g, last.b};
    }
    const detail::TurboEntry& a = detail::kTurboTable[index];
    const detail::TurboEntry& b = detail::kTurboTable[index + 1];
    const double f = scaled - static_cast<double>(index);
    return Rgb{clamp_byte(a.r + (b.r - a.r) * f), clamp_byte(a.g + (b.g - a.g) * f),
               clamp_byte(a.b + (b.b - a.b) * f)};
}

}  // namespace

Rgb sample(ColormapKind kind, double t) noexcept {
    const double x = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
    if (kind == ColormapKind::Turbo) return turbo_sample(x);
    const Fit& fit = kFits[static_cast<int>(kind)];
    Rgb out;
    for (int channel = 0; channel < 3; ++channel) {
        // 霍纳展开：系数表按 [power][channel] 存放。
        double value = 0.0;
        for (int power = 6; power >= 0; --power) {
            value = value * x + fit.c[power][channel];
        }
        const std::uint8_t v = clamp_byte(value * 255.0);
        if (channel == 0)
            out.r = v;
        else if (channel == 1)
            out.g = v;
        else
            out.b = v;
    }
    return out;
}

std::span<const Rgb> stops(ColormapKind kind, std::size_t count) {
    const std::size_t n = count == 0 ? 1 : count;
    return std::span<const Rgb>(cache().ensure(kind, n));
}

const char* colormap_name(ColormapKind kind) noexcept {
    switch (kind) {
        case ColormapKind::Viridis:
            return "viridis";
        case ColormapKind::Inferno:
            return "inferno";
        case ColormapKind::Magma:
            return "magma";
        case ColormapKind::Plasma:
            return "plasma";
        case ColormapKind::Turbo:
            return "turbo";
    }
    return "unknown";
}

}  // namespace termchart::color
