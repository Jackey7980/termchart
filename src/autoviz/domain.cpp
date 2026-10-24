// autoviz/domain.cpp —— 稳健 Y 轴域的 Tukey 篱实现（L21）
#include "termchart/autoviz/domain.hpp"

#include <algorithm>
#include <vector>

#include "termchart/data/aggregate.hpp"

namespace termchart::autoviz {
namespace {

// 域退化（裁完只剩一个值）时的补偿：上下各撑一点，让常数线落在中间而不是
// 触发比例尺 span=0。取 10% 或 1（对 0 附近的常数线不至于撑出个 2 倍域）。
void widen_degenerate(RobustDomain& d) noexcept {
    const double pad = std::max(std::abs(d.lo) * 0.1, 1.0);
    d.lo -= pad;
    d.hi += pad;
}

}  // namespace

RobustDomain robust_y_domain(std::span<const double> values, double fence) noexcept {
    RobustDomain d;
    std::vector<double> finite;
    finite.reserve(values.size());
    for (const double v : values) {
        if (std::isfinite(v)) {
            finite.push_back(v);
        }
    }
    if (finite.empty()) {
        return d;  // 全空：usable() == false，调用方退回数据自身域
    }
    const auto [lo_it, hi_it] = std::minmax_element(finite.begin(), finite.end());
    d.raw_lo = *lo_it;
    d.raw_hi = *hi_it;
    d.lo = d.raw_lo;
    d.hi = d.raw_hi;

    if (finite.size() < kOutlierMinSamples || !(fence > 0.0)) {
        // 样本太少/裁剪被关闭：不做统计判断，给出原始域即可（中位数仍给，
        // 因为告警/标题会用它做"主体在哪"的锚）。
        const data::Quartiles q =
            data::quartiles(data::DataView{finite.data(), finite.size()});
        d.median = q.median;
        return d;
    }

    const data::Quartiles q = data::quartiles(data::DataView{finite.data(), finite.size()});
    d.median = q.median;
    const double iqr = q.q3 - q.q1;
    double fence_lo = 0.0;
    double fence_hi = 0.0;
    if (iqr > 0.0) {
        fence_lo = q.q1 - fence * iqr;
        fence_hi = q.q3 + fence * iqr;
    } else {
        // IQR 退化（一半以上的值完全相同，典型："999 个 1 和 1 个 1e21"）：
        // 篱失去尺度，改用相对中位数的篱。绝对下限 1e-9 挡住 median=0 时
        // 篱宽收缩成 0、把所有非零值都判成离群的退化。
        const double spread = std::max(std::abs(q.median), 1e-9);
        fence_lo = q.median - fence * spread;
        fence_hi = q.median + fence * spread;
    }

    d.lo = std::max(d.raw_lo, fence_lo);
    d.hi = std::min(d.raw_hi, fence_hi);
    if (!(d.hi > d.lo)) {
        widen_degenerate(d);  // 篱内只剩一个值：撑开一点，保住可画的域
    }

    std::size_t high_count = 0;
    std::size_t low_count = 0;
    for (const double v : finite) {
        if (v > d.hi) {
            ++high_count;
        } else if (v < d.lo) {
            ++low_count;
        }
    }
    d.clipped = high_count + low_count;
    d.high = high_count >= low_count;
    return d;
}

RobustDomain merge_domains(const RobustDomain& a, const RobustDomain& b) noexcept {
    RobustDomain d;
    if (!a.usable() && !b.usable()) {
        return d;
    }
    if (!a.usable()) {
        return b;
    }
    if (!b.usable()) {
        return a;
    }
    d.raw_lo = std::min(a.raw_lo, b.raw_lo);
    d.raw_hi = std::max(a.raw_hi, b.raw_hi);
    d.lo = std::min(a.lo, b.lo);
    d.hi = std::max(a.hi, b.hi);
    d.clipped = a.clipped + b.clipped;
    // 中位数不取平均（跨系列的平均毫无意义）：报告裁得更多的那一侧。
    d.median = b.clipped > a.clipped ? b.median : a.median;
    d.high = a.clipped >= b.clipped ? a.high : b.high;
    return d;
}

}  // namespace termchart::autoviz
