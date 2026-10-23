// showcase/panorama_data.hpp —— Panorama 综合示例的共享复杂数据集（header-only）
//
// 目标：用"够难"的合成数据把库的能力跑一遍，并能从输出看出质量。数据全部由
// 固定种子确定（可复现），生成链路刻意覆盖：
//   util::Rng + 自建 fBm（库无噪声工具）→ math::stats / special / interpolate /
//   fft / linalg → data::rolling / ema / ohlc / histogram / quartiles →
//   io::csv 往返 → 相关矩阵（pearson）→ Lorenz 3D 云 → KDE 密度场（image::Bitmap）。
//
// 数据为何"难"：多尺度 fBm（5 octave）+ 制度漂移 + 0.6% 跳跃离群点 + 共享市场
// 因子（制造相关结构）→ 重尾、多尺度、彼此相关的 6 条价格路径，对坐标轴/图例/
// 颜色区分/采样都是压力测试。
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/data/aggregate.hpp"
#include "termchart/data/ohlc.hpp"
#include "termchart/data/rolling.hpp"
#include "termchart/io/csv.hpp"
#include "termchart/math/fft.hpp"
#include "termchart/math/interpolate.hpp"
#include "termchart/math/linalg.hpp"
#include "termchart/math/special.hpp"
#include "termchart/math/stats.hpp"
#include "termchart/math3d/vec3.hpp"
#include "termchart/util/random.hpp"

namespace tc = termchart;

namespace showcase {

using tc::Point;

// ── 自建分形噪声（库不提供噪声工具，按需用 Rng 造 value-noise + fBm）──────────
inline double smooth01(double t) { return t * t * (3.0 - 2.0 * t); }

inline double lattice(std::uint64_t seed, int xi, int yi) {
    const std::uint64_t h =
        seed ^ (static_cast<std::uint64_t>(static_cast<std::uint32_t>(xi)) * 0x9E3779B97F4A7C15ull) ^
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(yi)) * 0xC2B2AE3D27D4EB4Full);
    tc::util::Rng r(h);
    return r.unit() * 2.0 - 1.0;
}

inline double value_noise(std::uint64_t s, double x, double y) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const double fx = x - static_cast<double>(x0);
    const double fy = y - static_cast<double>(y0);
    const double v00 = lattice(s, x0, y0);
    const double v10 = lattice(s, x0 + 1, y0);
    const double v01 = lattice(s, x0, y0 + 1);
    const double v11 = lattice(s, x0 + 1, y0 + 1);
    const double sx = smooth01(fx);
    const double sy = smooth01(fy);
    return (v00 * (1.0 - sx) + v10 * sx) * (1.0 - sy) + (v01 * (1.0 - sx) + v11 * sx) * sy;
}

inline double fbm(std::uint64_t s, double x, double y, int oct = 5, double lac = 2.0,
                  double gain = 0.5) {
    double sum = 0.0;
    double amp = 1.0;
    double freq = 1.0;
    double norm = 0.0;
    for (int o = 0; o < oct; ++o) {
        sum += amp * value_noise(s, x * freq, y * freq);
        norm += amp;
        amp *= gain;
        freq *= lac;
    }
    return sum / norm;
}

// ── 数据集 ──────────────────────────────────────────────────────────────────
struct Instrument {
    std::string name;
    std::string tag;
    std::vector<double> price;   // M 点（起点 100）
    std::vector<double> ret;     // M-1 收益率
    std::vector<double> ema;     // EMA(20)，与 price 等长
    std::vector<double> smooth;  // CubicSpline 重采样，与 price 等长
    double mean_ret = 0.0;
    double sd_ret = 0.0;
    double ann_return = 0.0;
    double ann_vol = 0.0;
    double max_dd = 0.0;
    double last_vol = 0.0;       // 最近一个滚动标准差
};

struct Panorama {
    std::uint32_t seed = 20261022u;
    std::size_t M = 900;
    std::vector<double> t;                 // 0..M-1
    std::vector<Instrument> inst;          // 6 个
    std::vector<double> market;            // 等权指数（rebased 100）
    std::vector<double> market_smooth;     // 指数 CubicSpline 平滑

    std::vector<tc::data::OhlcBar> candles;  // inst[0] 的 K 线
    std::vector<double> volume;              // 每根 K 线成交量

    std::vector<double> pooled_ret;                    // 汇总收益（直方图）
    std::vector<tc::data::HistogramBin> ret_hist;
    std::vector<tc::data::Quartiles> boxes;            // 每个 instrument 一组（箱线）
    std::vector<double> box_lo;
    std::vector<double> box_hi;
    std::vector<std::vector<double>> box_outliers;
    std::vector<std::string> box_labels;

    std::vector<std::vector<double>> corr;             // NxN（ret 相关）
    std::vector<std::array<int, 2>> corr_edges;        // |corr|>0.5 的边

    std::vector<std::array<double, 3>> rr;             // x=ann_vol, y=ann_return, r=点大小
    std::vector<double> rr_x;
    std::vector<double> rr_y;
    double reg_slope = 0.0;
    double reg_intercept = 0.0;
    double rr_pearson = 0.0;

    std::vector<tc::math3d::Vec3> lorenz;              // Lorenz 吸引子 3D 云

    std::vector<Point> density_pop;                    // (波动, 收益) 混合分布点
    std::size_t kde_cells = 0;                         // KDE 场非空格数
    double kde_max = 0.0;                              // KDE 场峰值

    double gamma5 = 0.0;      // Γ(5) 应 = 24
    double erf1 = 0.0;        // erf(1)
    double bessel0 = 0.0;     // J0(1)
    double fft_peak_hz = 0.0; // 指数去趋势后的主频
    std::size_t csv_rows = 0; // CSV 往返校验行数
};

inline std::array<double, 3> normal3(tc::util::Rng& r, double mux, double muy, double mux2, double sd1,
                                     double sd2, double rho) {
    const double u1 = std::max(r.unit(), 1e-12);
    const double u2 = r.unit();
    const double z0 = std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2);
    const double z1 = std::sqrt(-2.0 * std::log(u1)) * std::sin(6.283185307179586 * u2);
    return {mux + sd1 * z0, muy + sd2 * (rho * z0 + std::sqrt(std::max(0.0, 1.0 - rho * rho)) * z1), mux2};
}

// 生成完整数据集（固定种子 → 完全可复现）
inline Panorama build(std::uint32_t seed) {
    Panorama p;
    p.seed = seed;
    const std::size_t M = p.M;
    p.t.resize(M);
    for (std::size_t i = 0; i < M; ++i) {
        p.t[i] = static_cast<double>(i);
    }

    static const std::array<const char*, 6> kNames = {"算力", "能源", "医药", "金融", "消费", "材料"};
    static const std::array<const char*, 6> kTags = {"CMP", "NRG", "PHR", "FIN", "CON", "MAT"};
    static const std::array<double, 6> kBeta = {1.25, 0.80, 0.45, 1.10, 0.65, 0.95};
    static const std::array<double, 6> kMu = {0.00035, 0.00018, 0.00022, 0.00028, 0.00015, 0.00020};

    // 共享市场因子：多尺度 fBm，制造跨品种相关结构。
    std::vector<double> mf(M);
    for (std::size_t i = 0; i < M; ++i) {
        mf[i] = fbm(seed, 0.0, static_cast<double>(i) * 0.02, 5);
    }

    p.inst.reserve(kNames.size());
    for (std::size_t k = 0; k < kNames.size(); ++k) {
        Instrument in;
        in.name = kNames[k];
        in.tag = kTags[k];
        in.price.resize(M);
        in.ret.resize(M - 1);
        tc::util::Rng jr(seed * 2654435761u + static_cast<std::uint64_t>(k) * 40503u + 7u);
        const std::uint64_t idseed = seed ^ (static_cast<std::uint64_t>(k) * 0x2545F4914F6CDD1Dull);
        double price = 100.0;
        in.price[0] = price;
        for (std::size_t i = 1; i < M; ++i) {
            const double dm = mf[i] - mf[i - 1];
            const double d_idio = fbm(idseed, 50.0, static_cast<double>(i) * 0.03, 4) -
                                  fbm(idseed, 50.0, static_cast<double>(i - 1) * 0.03, 4);
            double r = kMu[k] + kBeta[k] * dm * 1.4 + d_idio * 0.9;
            if (jr.unit() < 0.006) {  // 跳跃 / 离群点（重尾）
                r += jr.range(-1.0, 1.0) * 0.05;
            }
            in.ret[i - 1] = r;
            price *= (1.0 + r);
            in.price[i] = price;
        }
        in.ema = tc::data::ema(in.price, 20);
        const std::vector<double> vol =
            tc::data::rolling(in.ret, tc::data::RollingStat::Stddev, {.window = 30, .min_periods = 30});
        in.last_vol = vol.empty() ? 0.0 : vol.back();

        // 降采样 8 点做自然三次样条，再在全域重采样 → 平滑叠加线。
        std::vector<double> xs;
        std::vector<double> ys;
        for (std::size_t i = 0; i < M; i += 8) {
            xs.push_back(static_cast<double>(i));
            ys.push_back(in.price[i]);
        }
        if (xs.back() < static_cast<double>(M - 1)) {
            xs.push_back(static_cast<double>(M - 1));
            ys.push_back(in.price[M - 1]);
        }
        in.smooth.resize(M);
        const tc::math::CubicSpline sp(xs, ys);
        if (sp.valid()) {
            for (std::size_t i = 0; i < M; ++i) {
                in.smooth[i] = sp.at(static_cast<double>(i));
            }
        } else {
            in.smooth = in.price;
        }

        in.mean_ret = tc::math::mean(in.ret);
        in.sd_ret = tc::math::stddev(in.ret);
        in.ann_return = in.mean_ret * 252.0;
        in.ann_vol = in.sd_ret * std::sqrt(252.0);
        double peak = in.price[0];
        double dd = 0.0;
        for (std::size_t i = 0; i < M; ++i) {
            peak = std::max(peak, in.price[i]);
            dd = std::max(dd, (peak - in.price[i]) / peak);
        }
        in.max_dd = dd;
        p.inst.push_back(std::move(in));
    }

    // 等权指数（rebased 100）+ 平滑
    p.market.assign(M, 0.0);
    for (const Instrument& in : p.inst) {
        for (std::size_t i = 0; i < M; ++i) {
            p.market[i] += in.price[i];
        }
    }
    const double inv_n = 1.0 / static_cast<double>(p.inst.size());
    const double m0 = p.market[0] * inv_n;
    for (std::size_t i = 0; i < M; ++i) {
        p.market[i] = p.market[i] * inv_n / m0 * 100.0;
    }
    {
        std::vector<double> xs;
        std::vector<double> ys;
        for (std::size_t i = 0; i < M; i += 6) {
            xs.push_back(static_cast<double>(i));
            ys.push_back(p.market[i]);
        }
        xs.push_back(static_cast<double>(M - 1));
        ys.push_back(p.market[M - 1]);
        const tc::math::CubicSpline sp(xs, ys);
        p.market_smooth.resize(M);
        for (std::size_t i = 0; i < M; ++i) {
            p.market_smooth[i] = sp.valid() ? sp.at(static_cast<double>(i)) : p.market[i];
        }
    }

    // K 线 + 成交量（inst[0]）
    {
        std::vector<double> tick_t(M);
        for (std::size_t i = 0; i < M; ++i) {
            tick_t[i] = static_cast<double>(i);
        }
        tc::data::OhlcOptions oo;
        oo.bucket_seconds = 15.0;
        auto res = tc::data::ohlc(tick_t, p.inst[0].price, oo);
        if (res) {
            p.candles = std::move(*res);
        }
        p.volume.resize(p.candles.size());
        tc::util::Rng vr(seed ^ 0x51ED270Bu);
        for (std::size_t i = 0; i < p.candles.size(); ++i) {
            const double body = std::fabs(p.candles[i].close - p.candles[i].open);
            const double range = std::max(1e-9, p.candles[i].high - p.candles[i].low);
            p.volume[i] = 40.0 + 260.0 * (body / range) + vr.range(0.0, 60.0);
        }
    }

    // 汇总收益直方图 + 每品种箱线（四分位 + 1.5·IQR 离群点）
    for (const Instrument& in : p.inst) {
        for (double r : in.ret) {
            p.pooled_ret.push_back(r);
        }
    }
    p.ret_hist = tc::data::histogram({p.pooled_ret.data(), p.pooled_ret.size()}, 44);
    for (const Instrument& in : p.inst) {
        const tc::data::Quartiles q = tc::data::quartiles({in.ret.data(), in.ret.size()});
        p.boxes.push_back(q);
        const double iqr = q.q3 - q.q1;
        p.box_lo.push_back(q.q1 - 1.5 * iqr);
        p.box_hi.push_back(q.q3 + 1.5 * iqr);
        std::vector<double> outs;
        for (double r : in.ret) {
            if (r < q.q1 - 1.5 * iqr || r > q.q3 + 1.5 * iqr) {
                outs.push_back(r);
            }
        }
        p.box_outliers.push_back(std::move(outs));
        p.box_labels.push_back(in.tag);
    }

    // 相关矩阵 + 力导图边
    const std::size_t N = p.inst.size();
    p.corr.assign(N, std::vector<double>(N, 0.0));
    for (std::size_t i = 0; i < N; ++i) {
        for (std::size_t j = i; j < N; ++j) {
            const double c = (i == j) ? 1.0 : tc::math::pearson(p.inst[i].ret, p.inst[j].ret);
            p.corr[i][j] = c;
            p.corr[j][i] = c;
        }
    }
    for (std::size_t i = 0; i < N; ++i) {
        for (std::size_t j = i + 1; j < N; ++j) {
            if (std::fabs(p.corr[i][j]) > 0.5) {
                p.corr_edges.push_back({static_cast<int>(i), static_cast<int>(j)});
            }
        }
    }

    // 风险—收益散点 + 一元最小二乘（linalg::solve 解 2x2 正规方程）
    for (const Instrument& in : p.inst) {
        const double size = 12.0 + 70.0 * std::min(1.0, in.ann_vol / 0.05);
        p.rr.push_back({in.ann_vol, in.ann_return, size});
        p.rr_x.push_back(in.ann_vol);
        p.rr_y.push_back(in.ann_return);
    }
    {
        double sxx = 0.0;
        double sx = 0.0;
        double sxy = 0.0;
        double sy = 0.0;
        for (std::size_t i = 0; i < p.rr_x.size(); ++i) {
            sxx += p.rr_x[i] * p.rr_x[i];
            sx += p.rr_x[i];
            sxy += p.rr_x[i] * p.rr_y[i];
            sy += p.rr_y[i];
        }
        const double n = static_cast<double>(p.rr_x.size());
        const std::array<double, 4> A = {sxx, sx, sx, n};
        const std::array<double, 2> b = {sxy, sy};
        std::array<double, 2> sol = {0.0, 0.0};
        if (tc::math::solve({A.data(), A.size()}, {b.data(), b.size()}, 2, {sol.data(), sol.size()})) {
            p.reg_slope = sol[0];
            p.reg_intercept = sol[1];
        }
        p.rr_pearson = tc::math::pearson(p.rr_x, p.rr_y);
    }

    // Lorenz 吸引子（经典混沌系统，3D 云视觉压力测试）
    {
        double x = 0.1;
        double y = 0.0;
        double z = 0.0;
        const double dt = 0.006;
        const double sigma = 10.0;
        const double rho = 28.0;
        const double beta = 8.0 / 3.0;
        for (int i = 0; i < 800; ++i) {  // 丢弃瞬态
            const double dx = sigma * (y - x);
            const double dy = x * (rho - z) - y;
            const double dz = x * y - beta * z;
            x += dx * dt;
            y += dy * dt;
            z += dz * dt;
        }
        p.lorenz.reserve(3600);
        for (int i = 0; i < 3600; ++i) {
            const double dx = sigma * (y - x);
            const double dy = x * (rho - z) - y;
            const double dz = x * y - beta * z;
            x += dx * dt;
            y += dy * dt;
            z += dz * dt;
            p.lorenz.push_back(tc::math3d::Vec3{x, y, z - 25.0});  // z 居中
        }
    }

    // (波动, 收益) 混合分布点 + KDE 场（image::Bitmap 累积，见 TUI/GUI）
    {
        tc::util::Rng dr(seed ^ 0x9E3779B185EBCA87ull);
        p.density_pop.reserve(4200);
        for (int i = 0; i < 4200; ++i) {
            if (dr.unit() < 0.05) {  // 离群点
                const auto a = normal3(dr, 0.0, 0.0, 0.0, 1.2, 1.2, 0.0);
                p.density_pop.push_back(Point{a[0], a[1]});
                continue;
            }
            const double pick = dr.unit();
            if (pick < 0.45) {
                const auto a = normal3(dr, -0.5, 0.55, 0.0, 0.22, 0.30, 0.4);
                p.density_pop.push_back(Point{a[0], a[1]});
            } else if (pick < 0.8) {
                const auto a = normal3(dr, 0.6, 0.35, 0.0, 0.30, 0.22, -0.3);
                p.density_pop.push_back(Point{a[0], a[1]});
            } else {
                const auto a = normal3(dr, 1.1, -0.2, 0.0, 0.35, 0.35, 0.1);
                p.density_pop.push_back(Point{a[0], a[1]});
            }
        }
    }

    // 数学自检值（footer 展示）+ FFT 主频 + CSV 往返
    p.gamma5 = tc::math::gamma(5.0);
    p.erf1 = tc::math::erf(1.0);
    p.bessel0 = tc::math::bessel_j0(1.0);
    {
        const std::size_t want = tc::math::next_pow2(M);
        std::vector<double> re(want, 0.0);
        std::vector<double> im(want, 0.0);
        const double mean = tc::math::mean(p.market);
        for (std::size_t i = 0; i < M; ++i) {
            re[i] = p.market[i] - mean;  // 去趋势
        }
        static_cast<void>(tc::math::fft(re, im));
        double best = 0.0;
        std::size_t best_k = 0;
        for (std::size_t k = 1; k < want / 2; ++k) {
            const double mag = std::hypot(re[k], im[k]);
            if (mag > best) {
                best = mag;
                best_k = k;
            }
        }
        p.fft_peak_hz = static_cast<double>(best_k) / static_cast<double>(want);
    }

    return p;
}

// 价格矩阵 → CSV 文本（供 io::csv 往返校验）
inline std::string price_matrix_csv(const Panorama& p) {
    std::string s;
    s.reserve(p.M * 48);
    s += "t";
    for (const Instrument& in : p.inst) {
        s += ',';
        s += in.tag;
    }
    s += '\n';
    for (std::size_t i = 0; i < p.M; ++i) {
        s += std::to_string(static_cast<long long>(i));
        for (const Instrument& in : p.inst) {
            s += ',';
            s += std::to_string(in.price[i]);
        }
        s += '\n';
    }
    return s;
}

// 解析回表并返回行数（0 表示失败）
inline std::size_t csv_roundtrip_rows(const Panorama& p) {
    const std::string csv = price_matrix_csv(p);
    auto tbl = tc::io::parse_csv(csv);
    if (!tbl) {
        return 0;
    }
    return tbl->row_count;
}

}  // namespace showcase
