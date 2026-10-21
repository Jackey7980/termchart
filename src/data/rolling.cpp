// rolling.cpp —— 滚动窗口聚合（O(N) 滑窗家族）
//
// 2026-10-22 重写：旧实现每点构造 vector + 全窗口重算（O(N×W)，Median 另带
// sort）。现按统计量分派到 O(N) 家族：
//   * Sum / Mean  ：滑动和（running sum）
//   * Stddev      ：Welford 加/减（数值稳定，O(N)）
//   * Min / Max   ：单调双端队列（O(N)）
//   * Median      ：双堆（对顶堆）+ 惰性删除（O(N log W)）
// 语义与旧实现逐字节兼容：窗口 [lo,hi) 内**非 NaN** 值参与，非 NaN 计数 <
// min_periods → NaN；right_aligned / 居中两种窗口公式不变。
// 两种模式下窗口边界都随 i 单调不减，故滑入/滑出累加器全程适用。
#include "termchart/data/rolling.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <iterator>
#include <queue>
#include <set>
#include <utility>
#include <vector>

namespace termchart::data {

const char* rolling_stat_name(RollingStat s) noexcept {
    switch (s) {
    case RollingStat::Mean:   return "mean";
    case RollingStat::Sum:    return "sum";
    case RollingStat::Stddev: return "stddev";
    case RollingStat::Min:    return "min";
    case RollingStat::Max:    return "max";
    case RollingStat::Median: return "median";
    }
    return "?";
}

namespace {

constexpr double kNaN = std::nan("");

// ── 滑窗累加器家族 ─────────────────────────────────────────────────────
// 共同契约：add/remove 由调用方按窗口滑动驱动；NaN 值在调用方侧跳过，
// 各累加器只收非 NaN。count() 是非 NaN 计数（min_periods 判定用）。

class SlidingSum {
public:
    void add(double v) { sum_ += v; ++count_; }
    void remove(double v) { sum_ -= v; --count_; }
    [[nodiscard]] double sum() const { return sum_; }
    [[nodiscard]] double mean() const {
        return count_ > 0 ? sum_ / static_cast<double>(count_) : kNaN;
    }
    [[nodiscard]] std::size_t count() const { return count_; }

private:
    double sum_ = 0.0;
    std::size_t count_ = 0;
};

// Welford 滑动版：add 用新计数递推，remove 用逆公式退元素。
class SlidingWelford {
public:
    void add(double x) {
        ++n_;
        const double d = x - mean_;
        mean_ += d / static_cast<double>(n_);
        m2_ += d * (x - mean_);
    }
    void remove(double x) {
        if (n_ == 0) return;
        if (n_ == 1) {  // 退到空：复位（避免除零与残留）
            n_ = 0;
            mean_ = 0.0;
            m2_ = 0.0;
            return;
        }
        const double d = x - mean_;
        mean_ -= d / static_cast<double>(n_ - 1);
        m2_ -= d * (x - mean_);
        --n_;
    }
    [[nodiscard]] double stddev() const {
        // n≤1：单点无离散，方差恒为 0（否则 m2_ 的浮点残差会给出 ~1e-6 而非 0）。
        if (n_ <= 1) return 0.0;
        // m2_ 在加/减过程中可能因浮点抵消略小于 0；钳到 0（方差非负有理有据）。
        return std::sqrt(std::max(0.0, m2_ / static_cast<double>(n_)));
    }
    [[nodiscard]] std::size_t count() const { return n_; }

private:
    double mean_ = 0.0;
    double m2_ = 0.0;
    std::size_t n_ = 0;
};

// 单调双端队列：want_max=true 维护递减队列（队首=窗口最大），false 递增。
// 队列存 (下标, 值)；窗口左端滑过某下标时该候选自然过期。
class MonotonicQueue {
public:
    explicit MonotonicQueue(bool want_max) : want_max_(want_max) {}

    void add(std::size_t idx, double v) {
        while (!dq_.empty() &&
               (want_max_ ? dq_.back().second <= v : dq_.back().second >= v)) {
            dq_.pop_back();
        }
        dq_.emplace_back(idx, v);
    }
    // 窗口左端移到 lo：下标 < lo 的候选全部过期。
    void evict_below(std::size_t lo) {
        while (!dq_.empty() && dq_.front().first < lo) {
            dq_.pop_front();
        }
    }
    [[nodiscard]] bool empty() const { return dq_.empty(); }
    [[nodiscard]] double front_value() const { return dq_.front().second; }

private:
    bool want_max_;
    std::deque<std::pair<std::size_t, double>> dq_;
};

// 有序 multiset + 中位数指针（滑动窗口中位数，O(N log W)）。
// 选它而非双堆+惰性删除：双堆按"值 <= 堆顶"记账归属，在"元素埋在堆中间且
// 栈顶已因删除而改变"时会错判归属（甚至对空堆取 top = UB）；multiset 严格
// 正确、插入删除都是 O(log W)，中位数指针随规模平移，总复杂度 O(N log W)，
// 比旧实现 O(N·W·log W) 好一个 W 因子。mid_ 约定：奇数规模指正中，
// 偶数规模指左中。
// 有序双 multiset（滑动窗口中位数，O(N log W)）。
// 选它而非"单 multiset + 手写中位数指针"：手写指针在增删时极容易指向被 erase
// 的元素（悬垂），返回一个根本不在窗口里的数；双 multiset 用 lo_/hi_ 两个容器各
// 存一半、始终不变量 lo_.size() >= hi_.size() 且 lo 的最大 <= hi 的最小，median
// 直接取边界，无任何悬垂迭代器。erase 走 find 返回的局部迭代器，绝不跨增删持有。
class OrderedMedian {
private:
    std::multiset<double> lo_;  // 较小一半；size >= hi_，最大在 rbegin()
    std::multiset<double> hi_;  // 较大一半；最小在 begin()

    void rebalance() {
        while (lo_.size() > hi_.size() + 1) {
            auto it = std::prev(lo_.end());
            hi_.insert(*it);
            lo_.erase(it);
        }
        while (hi_.size() > lo_.size()) {
            auto it = hi_.begin();
            lo_.insert(*it);
            hi_.erase(it);
        }
    }

public:
    void add(double x) {
        if (lo_.empty() || x <= *lo_.rbegin()) {
            lo_.insert(x);
        } else {
            hi_.insert(x);
        }
        rebalance();
    }
    void remove(double x) {
        if (lo_.empty() && hi_.empty()) return;
        // 归属：x 在 lo 当且仅当 x <= lo 的最大（不变量保证不会误判到 hi）。
        if (!lo_.empty() && x <= *lo_.rbegin()) {
            auto it = lo_.find(x);
            if (it != lo_.end()) lo_.erase(it);
        } else {
            auto it = hi_.find(x);
            if (it != hi_.end()) hi_.erase(it);
        }
        rebalance();
    }
    [[nodiscard]] double median() const {
        const std::size_t n = lo_.size() + hi_.size();
        if (n == 0) return kNaN;
        if (n % 2 == 1) return *lo_.rbegin();
        return 0.5 * (*lo_.rbegin() + *hi_.begin());
    }
    [[nodiscard]] std::size_t count() const { return lo_.size() + hi_.size(); }
};

}  // namespace

std::vector<double>
rolling(const std::vector<double>& values, RollingStat stat,
        const RollingOptions& options) noexcept {
    std::vector<double> out;
    if (values.empty() || options.window == 0) return out;
    out.reserve(values.size());
    const std::size_t w = options.window;
    const std::size_t min_p = options.min_periods == 0 ? w : options.min_periods;

    SlidingSum sum;
    SlidingWelford welford;
    MonotonicQueue mono(stat == RollingStat::Max);
    OrderedMedian ordered;
    std::size_t mono_count = 0;  // Min/Max 的非 NaN 计数（单调队列自身不含）

    std::size_t lo = 0;  // 当前窗口 [lo, hi)
    std::size_t hi = 0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        std::size_t tlo = 0;
        std::size_t thi = 0;
        if (options.right_aligned) {
            tlo = (i + 1 >= w) ? (i + 1 - w) : 0;
            thi = i + 1;
        } else {
            const std::size_t half = w / 2;
            tlo = (i > half) ? (i - half) : 0;
            thi = std::min(values.size(), i + half + 1);
        }
        // 窗口单调右移：滑入 [hi, thi)，滑出 [lo, tlo)。
        while (hi < thi) {
            const double v = values[hi];
            if (!std::isnan(v)) {
                switch (stat) {
                case RollingStat::Mean:
                case RollingStat::Sum:    sum.add(v); break;
                case RollingStat::Stddev: welford.add(v); break;
                case RollingStat::Min:
                case RollingStat::Max:    mono.add(hi, v); ++mono_count; break;
                case RollingStat::Median: ordered.add(v); break;
                }
            }
            ++hi;
        }
        while (lo < tlo) {
            const double v = values[lo];
            if (!std::isnan(v)) {
                switch (stat) {
                case RollingStat::Mean:
                case RollingStat::Sum:    sum.remove(v); break;
                case RollingStat::Stddev: welford.remove(v); break;
                case RollingStat::Min:
                case RollingStat::Max:    --mono_count; break;  // 过期由 evict 统一清
                case RollingStat::Median: ordered.remove(v); break;
                }
            }
            ++lo;
        }
        if (stat == RollingStat::Min || stat == RollingStat::Max) {
            mono.evict_below(tlo);
        }

        std::size_t count = 0;
        double result = kNaN;
        switch (stat) {
        case RollingStat::Mean:   count = sum.count();     result = sum.mean(); break;
        case RollingStat::Sum:    count = sum.count();     result = sum.sum(); break;
        case RollingStat::Stddev: count = welford.count(); result = welford.stddev(); break;
        case RollingStat::Min:
        case RollingStat::Max:
            count = mono_count;
            result = mono.empty() ? kNaN : mono.front_value();
            break;
        case RollingStat::Median: count = ordered.count(); result = ordered.median(); break;
        }

        out.push_back(count < min_p ? kNaN : result);
    }
    return out;
}

std::vector<double> ema(const std::vector<double>& values, std::size_t window) noexcept {
    std::vector<double> out;
    if (values.empty() || window == 0) return out;
    out.reserve(values.size());
    const double alpha = 2.0 / (static_cast<double>(window) + 1.0);
    bool seeded = false;
    double prev = 0.0;
    for (const double v : values) {
        if (std::isnan(v)) {
            out.push_back(std::nan(""));
            continue;
        }
        if (!seeded) {
            prev = v;
            seeded = true;
            out.push_back(prev);
            continue;
        }
        prev = alpha * v + (1.0 - alpha) * prev;
        out.push_back(prev);
    }
    return out;
}

}  // namespace termchart::data
