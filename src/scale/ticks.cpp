#include "termchart/scale/ticks.hpp"

#include <algorithm>
#include <cmath>

#include "termchart/scale/nice.hpp"
#include "termchart/util/math.hpp"

namespace termchart::scale {

std::vector<double> nice_ticks(double domain_min, double domain_max, int target) noexcept {
    std::vector<double> out;
    if (!util::is_finite(domain_min) || !util::is_finite(domain_max) || target <= 0) {
        return out;
    }
    if (domain_min > domain_max) {
        std::swap(domain_min, domain_max);
    }
    if (domain_min == domain_max) {
        out.push_back(domain_min);
        return out;
    }

    const double step = nice_step(domain_max - domain_min, target);
    if (!(step > 0.0)) {
        return out;
    }
    // 第一个不小于 domain_min 的整步；+1e-9 容差避免浮点把边界刻度漏掉。
    const double first = std::ceil(domain_min / step - 1e-9) * step;
    for (double v = first; v <= domain_max + step * 1e-9; v += step) {
        out.push_back(v);
    }
    return out;
}

std::vector<double> ticks_for(const Scale& scale, int target) noexcept {
    // 对称对数轴：等差采样 + inverse 会让刻度在屏幕上挤成一团/拉成深渊，
    // 必须用专用生成器（线性区插细分 + 对数区按数量级铺）。
    if (const auto* sl = dynamic_cast<const SymLogScale*>(&scale)) {
        return symlog_ticks(*sl, target);
    }
    std::vector<double> out;
    if (!scale.valid() || target <= 0) {
        return out;
    }
    const double rmin = scale.range_min();
    const double rmax = scale.range_max();
    // y 轴的 range 必然反向（数据大在上方 → 行号小），所以退化和边界检查都
    // 必须与方向无关：写死 rmin<rmax 会让**所有**笛卡尔图的 y 刻度恒为空。
    const double span = std::fabs(rmax - rmin);
    if (span == 0.0) {
        return out;
    }
    const double rlo = std::min(rmin, rmax);
    const double rhi = std::max(rmin, rmax);

    const double dmin = scale.domain_min();
    const double dmax = scale.domain_max();
    const double step = nice_step(dmax - dmin, target);
    if (!(step > 0.0)) {
        return out;
    }
    // +0.0：ceil(-1e-9) = -0.0，乘出来是 -0，标签会渲染成 "-0"（-0 + 0 = +0）。
    const double first = std::ceil(dmin / step - 1e-9) * step + 0.0;
    for (double v = first; v <= dmax + step * 1e-9; v += step) {
        const double pixel = scale.forward(v);
        if (pixel >= rlo - 1e-9 && pixel <= rhi + 1e-9) {
            out.push_back(v);
        }
    }
    return out;
}

std::vector<double> symlog_ticks(const SymLogScale& scale, int target) noexcept {
    std::vector<double> out;
    if (!scale.valid() || target <= 0) {
        return out;
    }
    double dmin = scale.domain_min();
    double dmax = scale.domain_max();
    if (dmin > dmax) std::swap(dmin, dmax);
    if (dmin == dmax) {
        out.push_back(dmin);
        return out;
    }
    const double C = (scale.linthresh() > 0.0) ? scale.linthresh() : 1.0;
    const double absmax = std::max(std::fabs(dmin), std::fabs(dmax));

    // 零线（若落在域内）。
    if (dmin <= 0.0 && dmax >= 0.0) out.push_back(0.0);

    // 线性区 [-C, C] ∩ 域：插几个 nice 细分（0 已单独加，不重复）。
    {
        const double lo = std::max(dmin, -C);
        const double hi = std::min(dmax, C);
        if (lo < hi) {
            const auto lt = nice_ticks(lo, hi, std::max(2, target / 3));
            for (double v : lt) {
                if (v == 0.0) continue;
                out.push_back(v);
            }
        }
    }
    // 线性区边界 ±C（落在域内才加；与上方线性细分去重靠末尾 unique）。
    if (-C >= dmin - 1e-9 && -C <= dmax + 1e-9) out.push_back(-C);
    if ( C >= dmin - 1e-9 &&  C <= dmax + 1e-9) out.push_back( C);

    // 对数区 |x| > C：按数量级铺刻度（C, 2C, 5C, 10C, ...），正负对称。
    int decades = 0;
    double mag = C;
    while (mag <= absmax * (1.0 + 1e-9) && decades < 16) {
        for (double m : {mag, 2.0 * mag, 5.0 * mag}) {
            if (m <= C - 1e-9) continue;
            if (dmax > C - 1e-9 && m <= dmax + 1e-9) out.push_back(m);
            if (dmin < -(C - 1e-9) && m <= std::fabs(dmin) + 1e-9) out.push_back(-m);
        }
        mag *= 10.0;
        ++decades;
    }

    // 升序 + 去重（±C 可能在两段都出现）。
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end(),
                          [](double a, double b) { return std::fabs(a - b) < 1e-9; }),
              out.end());

    // 极端跨数量级数据防爆：超量则等距抽稀到约 2×target。
    const std::size_t cap = static_cast<std::size_t>(2 * target) + 8;
    if (out.size() > cap) {
        std::vector<double> thin;
        thin.reserve(cap);
        const double stride = static_cast<double>(out.size() - 1) /
                             static_cast<double>(cap - 1);
        for (std::size_t i = 0; i < cap; ++i) {
            thin.push_back(out[static_cast<std::size_t>(std::round(i * stride))]);
        }
        out = std::move(thin);
    }
    return out;
}

}  // namespace termchart::scale
