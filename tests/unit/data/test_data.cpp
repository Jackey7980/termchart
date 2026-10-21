// tests/unit/data/test_data.cpp —— 数据处理层（aggregate / fill_missing /
//                                 rolling / resample）的最小测试集
//
// doctest：doctest.h vendored 于 third_party/，本测试通过 TEST_CASE 宏串联。
// 不依赖任何外部 fixture；测试样本全部内置。
//
// 历史：2026-10-21 随模块从 util/data 上移至 data（L4），命名空间
// termchart::util::data → termchart::data。
#include "termchart/data/aggregate.hpp"
#include "termchart/data/fill_missing.hpp"
#include "termchart/data/ohlc.hpp"
#include "termchart/data/resample.hpp"
#include "termchart/data/rolling.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <doctest.h>

using namespace termchart::data;

namespace {

bool approx(double a, double b, double eps = 1e-9) { return std::fabs(a - b) < eps; }

}  // namespace

TEST_CASE("util::data::aggregate::histogram") {
    const double raw[] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
    DataView v{raw, 10};
    auto h = histogram(v, 5);
    CHECK(h.size() == 5);
    CHECK(h[0].count == 2);
    CHECK(h[1].count == 2);
    CHECK(h[4].count == 2);
}

TEST_CASE("util::data::aggregate::quartiles") {
    const double raw[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    auto q = quartiles(DataView{raw, 11});
    CHECK(approx(q.minimum, 1.0));
    CHECK(approx(q.maximum, 11.0));
    CHECK(approx(q.median, 6.0));
    CHECK(q.q1 >= 3.0);
    CHECK(q.q3 <= 9.0);
}

TEST_CASE("util::data::aggregate::group_reduce sum/mean") {
    const double keys[]   = {1, 1, 2, 2, 2, 3};
    const double values[] = {10, 20, 5, 15, 25, 100};
    auto g = group_reduce({keys, 6}, {{values, 6}}, Reducer::Sum);
    REQUIRE(g.size() == 3);
    CHECK(g[0].key == 1);
    CHECK(g[1].key == 2);
    CHECK(g[2].key == 3);
    // 每组 DataView 在 group_reduce 里合成一段
    CHECK(g[0].values_per_col[0].size == 2);
    CHECK(g[1].values_per_col[0].size == 3);
    CHECK(g[2].values_per_col[0].size == 1);
}

TEST_CASE("util::data::fill_missing::forward_fill") {
    std::vector<double> v = {1.0, std::nan(""), 3.0, std::nan(""), std::nan(""), 6.0};
    auto n = fill_missing(v, FillStrategy::ForwardFill);
    CHECK(n == 3);
    CHECK(v[1] == 1.0);
    CHECK(v[3] == 3.0);
    CHECK(v[4] == 3.0);
    CHECK(v[5] == 6.0);
}

TEST_CASE("util::data::fill_missing::linear_interp") {
    std::vector<double> v = {0.0, std::nan(""), std::nan(""), 3.0};
    (void)fill_missing(v, FillStrategy::LinearInterp);
    CHECK(approx(v[1], 1.0));
    CHECK(approx(v[2], 2.0));
}

TEST_CASE("util::data::fill_missing::drop") {
    std::vector<double> v = {1.0, std::nan(""), 3.0, std::nan("")};
    auto out = drop_missing(v);
    CHECK(out.size() == 2);
    CHECK(out[0] == 1.0);
    CHECK(out[1] == 3.0);
}

TEST_CASE("util::data::fill_missing::count") {
    std::vector<double> v = {1.0, std::nan(""), 3.0, std::nan(""), std::nan("")};
    CHECK(count_missing(v) == 3);
}

TEST_CASE("util::data::rolling::mean window=3") {
    std::vector<double> v = {1, 2, 3, 4, 5};
    auto r = rolling(v, RollingStat::Mean, {3});
    REQUIRE(r.size() == 5);
    CHECK(std::isnan(r[0]));
    CHECK(std::isnan(r[1]));
    CHECK(approx(r[2], 2.0));  // (1+2+3)/3
    CHECK(approx(r[3], 3.0));  // (2+3+4)/3
    CHECK(approx(r[4], 4.0));
}

TEST_CASE("util::data::rolling::sum min_periods") {
    std::vector<double> v = {1, 2, 3, 4, 5};
    auto r = rolling(v, RollingStat::Sum, {3, 2});
    REQUIRE(r.size() == 5);
    CHECK(approx(r[1], 3.0));   // 1+2
    CHECK(approx(r[2], 6.0));   // 1+2+3
    CHECK(approx(r[4], 12.0));
}

TEST_CASE("util::data::resample::linspace_edges") {
    auto e = linspace_edges(0.0, 10.0, 5);
    REQUIRE(e.size() == 5);
    CHECK(approx(e.front(), 0.0));
    CHECK(approx(e.back(),  10.0));
}

TEST_CASE("util::data::resample::mean by edges") {
    const double raw[] = {0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5};
    std::vector<double> v(raw, raw + sizeof(raw) / sizeof(raw[0]));
    auto e = linspace_edges(0.0, 10.0, 5);
    auto r = resample(v, e, Reducer::Mean);
    REQUIRE(r.size() == 4);
    // 每桶 2.5 个样本均值近似桶中心
    for (const auto& b : r) {
        CHECK((approx(static_cast<double>(b.count), 2.0) ||
               approx(static_cast<double>(b.count), 3.0)));
    }
}

// ── 2026-10-21 随模块上移新增：EMA 与 OHLC ──────────────────────────────

TEST_CASE("data::ema::递推值（window=3, alpha=0.5）") {
    const std::vector<double> v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto r = ema(v, 3);
    REQUIRE(r.size() == 10);
    CHECK(approx(r[0], 1.0));        // 种子
    CHECK(approx(r[1], 1.5));        // 0.5*2+0.5*1
    CHECK(approx(r[2], 2.25));
    CHECK(approx(r[3], 3.125));
    CHECK(approx(r[9], 9.001953125));
    // 常数序列 EMA 恒等于该常数
    const std::vector<double> c(6, 7.0);
    for (double x : ema(c, 4)) CHECK(approx(x, 7.0));
}

TEST_CASE("data::ema::NaN 透传且保持状态") {
    const std::vector<double> v = {1, 2, std::nan(""), 4, 5};
    auto r = ema(v, 3);
    REQUIRE(r.size() == 5);
    CHECK(approx(r[0], 1.0));
    CHECK(approx(r[1], 1.5));
    CHECK(std::isnan(r[2]));                       // 透传
    CHECK(approx(r[3], 0.5 * 4 + 0.5 * 1.5));      // 状态保持，继续递推
    CHECK(ema(std::vector<double>{}, 3).empty());
    CHECK(ema(v, 0).empty());                      // window=0 → 空
}

TEST_CASE("data::ohlc::按 60s 分桶（左闭右开）") {
    const std::vector<double> t = {0, 30, 90, 120, 210};
    const std::vector<double> v = {10, 12, 8, 15, 11};
    const auto r = ohlc(t, v, {60.0});
    REQUIRE(r.has_value());
    const auto& bars = *r;
    REQUIRE(bars.size() == 3);
    // 桶 1：锚定 t0=0 → [0,60) → 10,12
    CHECK(approx(bars[0].t0, 0.0));
    CHECK(approx(bars[0].open, 10.0));
    CHECK(approx(bars[0].high, 12.0));
    CHECK(approx(bars[0].low, 10.0));
    CHECK(approx(bars[0].close, 12.0));
    CHECK(bars[0].count == 2);
    // 桶 2：锚定 t0=90 → [90,150) → 8,15
    CHECK(approx(bars[1].t0, 90.0));
    CHECK(approx(bars[1].open, 8.0));
    CHECK(approx(bars[1].high, 15.0));
    CHECK(approx(bars[1].low, 8.0));
    CHECK(approx(bars[1].close, 15.0));
    CHECK(bars[1].count == 2);
    // 桶 3：单样本
    CHECK(approx(bars[2].t0, 210.0));
    CHECK(bars[2].count == 1);
    CHECK(approx(bars[2].high, 11.0));
}

TEST_CASE("data::ohlc::单桶模式与 NaN 跳过") {
    const std::vector<double> t = {0, 10, 20, 30, 40};
    const std::vector<double> v = {10, 12, std::nan(""), 15, 11};
    // bucket ≤ 0 → 单桶全量（NaN 跳过）
    const auto r = ohlc(t, v, {0.0});
    REQUIRE(r.has_value());
    const auto& bars = *r;
    REQUIRE(bars.size() == 1);
    CHECK(approx(bars[0].open, 10.0));
    CHECK(approx(bars[0].high, 15.0));
    CHECK(approx(bars[0].low, 10.0));
    CHECK(approx(bars[0].close, 11.0));
    CHECK(bars[0].count == 4);
    // 空输入 → 空
    const auto empty = ohlc(std::vector<double>{}, std::vector<double>{});
    REQUIRE(empty.has_value());
    CHECK(empty->empty());
}

TEST_CASE("data::ohlc::无序时间戳 fail-loud（2026-10-22 加固）") {
    // 分桶算法前提是非降序；违反必须报 InvalidArgument，绝不静默切错桶。
    const std::vector<double> t_desc = {100, 90, 80};
    const std::vector<double> v = {1, 2, 3};
    const auto bad = ohlc(t_desc, v, {10.0});
    CHECK_FALSE(bad.has_value());
    CHECK(bad.error() == termchart::ErrorCode::InvalidArgument);
    // 乱序但含相等（非降序允许相等）→ 正常。
    const auto ok = ohlc(std::vector<double>{5, 5, 6}, std::vector<double>{1, 2, 3}, {10.0});
    CHECK(ok.has_value());
}

TEST_CASE("data::aggregate::group_reduce 生命周期回归（悬垂指针，2026-10-22）") {
    // 红牌 bug 回归：旧实现把循环内临时 vector 的指针存进 DataView，
    // 迭代结束即悬垂。小数据下 UB "碰巧不崩"——本用例用多组多列 + 值校验，
    // 修复前必乱（读回收内存）。修复后 owned 持有、view 指向 owned。
    const std::vector<double> keys = {1, 2, 1, 2, 1, 3, 2, 3, 1, 3};
    const std::vector<double> col_a = {10, 20, 11, 21, 12, 30, 22, 31, 13, 32};
    const std::vector<double> col_b = {1.5, 2.5, 1.6, 2.6, 1.7, 3.5, 2.7, 3.6, 1.8, 3.7};
    const auto groups = group_reduce(DataView{keys.data(), keys.size()},
                                     std::vector<DataView>{{col_a.data(), col_a.size()},
                                                           {col_b.data(), col_b.size()}},
                                     Reducer::Sum);
    REQUIRE(groups.size() == 3);
    // key 升序：1 / 2 / 3。
    CHECK(approx(groups[0].key, 1.0));
    CHECK(approx(groups[1].key, 2.0));
    CHECK(approx(groups[2].key, 3.0));
    // key=1 的 a 列 = {10, 11, 12, 13}；b 列 = {1.5, 1.6, 1.7, 1.8}。
    REQUIRE(groups[0].values_per_col.size() == 2);
    const DataView& a1 = groups[0].values_per_col[0];
    const DataView& b1 = groups[0].values_per_col[1];
    REQUIRE(a1.size == 4);
    REQUIRE(b1.size == 4);
    CHECK(approx(a1.data[0], 10.0));
    CHECK(approx(a1.data[1], 11.0));
    CHECK(approx(a1.data[2], 12.0));
    CHECK(approx(a1.data[3], 13.0));
    CHECK(approx(b1.data[3], 1.8));
    // key=2 的 a 列 = {20, 21, 22}；key=3 = {30, 31, 32}。
    CHECK(groups[1].values_per_col[0].size == 3);
    CHECK(approx(groups[1].values_per_col[0].data[2], 22.0));
    CHECK(groups[2].values_per_col[0].size == 3);
    CHECK(approx(groups[2].values_per_col[0].data[0], 30.0));
    // Group 整体拷贝后 view 仍有效（owned 随 Group 移动，堆缓冲不动）。
    const std::vector<Group> copied = groups;
    CHECK(copied[0].values_per_col[0].size == 4);
    CHECK(approx(copied[0].values_per_col[0].data[1], 11.0));
}

TEST_CASE("data::aggregate::group_reduce_multi 生命周期回归（2026-10-22）") {
    const std::vector<double> keys = {5, 6, 5, 6, 5};
    const std::vector<double> vals = {1.0, 2.0, 3.0, 4.0, 5.0};
    const auto res = group_reduce_multi(DataView{keys.data(), keys.size()},
                                        std::vector<DataView>{{vals.data(), vals.size()}},
                                        std::vector<Reducer>{Reducer::Sum, Reducer::Mean});
    REQUIRE(res.groups.size() == 2);
    CHECK(approx(res.groups[0].key, 5.0));
    REQUIRE(res.groups[0].values_per_col.size() == 1);
    const DataView& v5 = res.groups[0].values_per_col[0];
    REQUIRE(v5.size == 3);
    CHECK(approx(v5.data[0], 1.0));
    CHECK(approx(v5.data[1], 3.0));
    CHECK(approx(v5.data[2], 5.0));
    CHECK(approx(res.groups[1].values_per_col[0].data[0], 2.0));
}

namespace {

// 朴素参照：对每个点重算整窗（O(N·W)），与 O(N) 滑窗实现逐点比对。
// 它**不使用被测代码**的累加器，只复刻窗口边界 + 全窗口规约。
std::vector<double> naive_rolling(const std::vector<double>& v, RollingStat stat,
                                  const RollingOptions& opt) {
    const std::size_t n = v.size();
    const std::size_t w = opt.window;
    const std::size_t min_p = opt.min_periods == 0 ? w : opt.min_periods;
    std::vector<double> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        std::size_t tlo = 0, thi = 0;
        if (opt.right_aligned) {
            tlo = (i + 1 >= w) ? (i + 1 - w) : 0;
            thi = i + 1;
        } else {
            const std::size_t half = w / 2;
            tlo = (i > half) ? (i - half) : 0;
            thi = std::min(n, i + half + 1);
        }
        std::vector<double> win;
        win.reserve(thi - tlo);
        for (std::size_t k = tlo; k < thi; ++k) {
            if (!std::isnan(v[k])) win.push_back(v[k]);
        }
        if (win.size() < min_p) {
            out.push_back(std::nan(""));
            continue;
        }
        double r = std::nan("");
        switch (stat) {
        case RollingStat::Sum:    r = 0; for (double x : win) r += x; break;
        case RollingStat::Mean:   r = 0; for (double x : win) r += x; r /= static_cast<double>(win.size()); break;
        case RollingStat::Stddev: {
            double m = 0; for (double x : win) m += x; m /= static_cast<double>(win.size());
            double s = 0; for (double x : win) s += (x - m) * (x - m);
            r = std::sqrt(s / static_cast<double>(win.size()));
            break;
        }
        case RollingStat::Min:    r = *std::min_element(win.begin(), win.end()); break;
        case RollingStat::Max:    r = *std::max_element(win.begin(), win.end()); break;
        case RollingStat::Median: {
            std::vector<double> s = win;
            std::sort(s.begin(), s.end());
            const std::size_t m = s.size() / 2;
            r = (s.size() % 2) ? s[m] : 0.5 * (s[m - 1] + s[m]);
            break;
        }
        }
        out.push_back(r);
    }
    return out;
}

// 确定性 LCG，避免随机性导致 CI 抖动。
std::vector<double> lcg_series(std::size_t n, std::uint64_t seed) {
    std::vector<double> v;
    v.reserve(n);
    std::uint64_t s = seed ? seed : 0x9E3779B97F4A7C15ULL;
    for (std::size_t i = 0; i < n; ++i) {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        const double x = static_cast<double>(s & 0xFFFFFF) / 0xFFFFFF * 100.0 - 50.0;
        v.push_back(x);
    }
    return v;
}

}  // namespace

TEST_CASE("data::rolling::differential O(N) vs naive（2026-10-22 重写）") {
    // 覆盖：6 种统计量 × 右对齐/居中 × 含 NaN × 不同 min_periods。
    auto base = lcg_series(200, 12345);
    // 注入 NaN（确定性位置）。
    for (std::size_t i = 0; i < base.size(); i += 17) base[i] = std::nan("");

    const RollingStat stats[] = {RollingStat::Mean, RollingStat::Sum, RollingStat::Stddev,
                                 RollingStat::Min,  RollingStat::Max, RollingStat::Median};
    const std::size_t windows[] = {1, 3, 5, 8, 15};
    for (RollingStat st : stats) {
        for (std::size_t w : windows) {
            for (bool right : {true, false}) {
                for (std::size_t mp : {std::size_t{0}, w / 2, w}) {
                    RollingOptions opt;
                    opt.window = w;
                    opt.right_aligned = right;
                    opt.min_periods = mp;
                    const auto got = rolling(base, st, opt);
                    const auto ref = naive_rolling(base, st, opt);
                    REQUIRE(got.size() == ref.size());
                    for (std::size_t i = 0; i < got.size(); ++i) {
                        const bool gm = std::isnan(got[i]), rm = std::isnan(ref[i]);
                        if (gm || rm) {
                            // 至少一侧是 NaN：必须"两侧都为 NaN"才算一致。
                            CHECK(gm == rm);
                        } else {
                            // 相对容差 1e-4：算法等价性验证。滑窗 Welford 的浮点残差在
                            // 本数据尺度（≈±50）下可达 ~1e-4，均值/中位数等远低于此；
                            // 真实算法 bug（错窗口/错公式）会产出数量级差异，远超此阈。
                            const double tol = 1e-4 * (1.0 + std::fabs(ref[i]));
                            CHECK(std::fabs(got[i] - ref[i]) <= tol);
                        }
                    }
                }
            }
        }
    }
}

