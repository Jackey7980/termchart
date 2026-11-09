// autoviz/domain.cpp —— 稳健 Y 轴域的 Tukey 篱实现（L21）
#include "termchart/autoviz/domain.hpp"

#include <algorithm>
#include <cmath>  // std::abs(double)：不能依赖 <algorithm> 的传递包含
#include <vector>

#include "termchart/math/stats.hpp"

namespace termchart::autoviz {
namespace {

// 域退化（裁完只剩一个值）时的补偿：上下各撑一点，让常数线落在中间而不是
// 触发比例尺 span=0。取 10% 或 1（对 0 附近的常数线不至于撑出个 2 倍域）。
void widen_degenerate(RobustDomain& d) noexcept {
    const double pad = std::max(std::abs(d.lo) * 0.1, 1.0);
    d.lo -= pad;
    d.hi += pad;
}

// 放弃裁剪：轴域退回原始 [min, max]，但 clipped/median/high 保留——
// 那是"检测到了脏值"的证据，告警要用它，不许因为不裁就装看不见。
void restore_raw(RobustDomain& d) noexcept {
    d.lo = d.raw_lo;
    d.hi = d.raw_hi;
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

    if (finite.size() < 2) {
        d.median = finite.front();  // 一个值谈不上分布，也谈不上离群
        return d;
    }
    if (!(fence > 0.0)) {
        return d;  // 裁剪被显式关闭：轴域就是 [min, max]，不判越界
    }

    // 篱的数学（含 IQR 退化时改用相对中位数的尺度）在 math::tukey_fence 里，
    // 这里只管策略：域取 [raw_lo, raw_hi] 与篱的交集，退化时撑开、越界过多则放弃。
    const math::TukeyFence f = math::tukey_fence(finite, fence);
    d.median = f.median;
    d.lo = std::max(d.raw_lo, f.lo);
    d.hi = std::min(d.raw_hi, f.hi);
    if (!(d.hi > d.lo)) {
        widen_degenerate(d);  // 篱内只剩一个值：撑开一点，保住可画的域
    }

    // 越界统计（诊断口径：先数，再决定敢不敢裁）。
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
    if (d.clipped == 0) {
        d.decision = ClipDecision::kNone;
        return d;
    }

    // 保护闸一：样本太少。3~9 个点裁掉 1 个就是 11%~33%，剩下的点被强行拉满
    // 整根轴——读者会以为波动剧烈，比略微压平更误导。
    if (finite.size() < kOutlierMinSamples) {
        restore_raw(d);
        d.decision = ClipDecision::kSmallSample;
        return d;
    }
    // 保护闸二：越界占比过高。那说明"整列本来就离散"，不是"少数脏值"，
    // 裁掉两成以上的点等于人为制造趋势。
    if (static_cast<double>(d.clipped) >
        kOutlierMaxClipRatio * static_cast<double>(finite.size())) {
        restore_raw(d);
        d.decision = ClipDecision::kTooMany;
        return d;
    }
    d.decision = ClipDecision::kApplied;
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
    // 并集后只要任一系列裁过，合并域就仍比合并原始域紧；是否真的更紧由
    // active() 的 lo/hi 判定兜底（并集可能把两边的极值都包回来）。
    d.decision = (a.active() || b.active()) ? ClipDecision::kApplied : ClipDecision::kNone;
    return d;
}

}  // namespace termchart::autoviz
