// showcase/catalog_charts.cpp —— 全图表类型 catalog（cartesian/polar/financial/
// statistical/hierarchical/geo/indicator/text/flow）+ 色彩 + math/data 曲线）。
// 每个 Demo 用统一 chart_demo 助手紧凑登记；数据为确定性小样本。
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "catalog.hpp"

#include "termchart/math/special.hpp"  // tc::math::erf/bessel_j0（termchart_full 不含 math 模块）

namespace cat {
namespace {

const Color kB = Color::rgb(90, 100, 120);
const Color kT = Color::rgb(120, 210, 255);

// xorshift 确定性伪随机 + Box-Muller 正态样本
inline double u01(std::uint64_t& s) {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return static_cast<double>(s >> 11) / static_cast<double>(1ull << 53);
}
std::vector<double> normals(std::size_t n, std::uint64_t seed = 88172645463325252ull) {
    std::vector<double> v;
    v.reserve(n);
    std::uint64_t s = seed | 1ull;
    for (std::size_t i = 0; i < n; ++i) {
        const double u1 = std::max(u01(s), 1e-12);
        const double u2 = u01(s);
        v.push_back(std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2));
    }
    return v;
}
std::vector<double> walk(std::size_t n, double start = 100.0, double vol = 0.02) {
    std::vector<double> v(n);
    std::uint64_t s = 1234567ull;
    double p = start;
    v[0] = p;
    for (std::size_t i = 1; i < n; ++i) {
        p *= 1.0 + (u01(s) - 0.5) * 2.0 * vol;
        v[i] = p;
    }
    return v;
}

// 文件级助手（供各 demo 的捕获无关 lambda 直接调用，避免局部量捕获导致悬垂）。
tc::chart::Series series(const char* name, Color col, const std::vector<Point>& pts) {
    tc::chart::Series s;
    s.name = name;
    s.color = col;
    s.points = pts;
    return s;
}

void register_charts_impl(std::vector<Demo>& d) {
    auto add = [&](const char* c, const char* n, std::function<CellBuffer(std::uint32_t, std::uint32_t)> f) {
        d.push_back(Demo{c, n, std::move(f)});
    };
    auto demo = [&](const char* c, const char* n, const char* t, auto body) {
        add(c, n, [t, body](std::uint32_t w, std::uint32_t h) {
            return cat::panel(w, h, t, [&body](tc::canvas::Canvas& cv) { body(cv); }, kB, kT);
        });
    };

    // ═══ 笛卡尔 cartesian ═══════════════════════════════════════════════
    demo("笛卡尔", "折线 Line", "LineChart", [](tc::canvas::Canvas& cv) {
        tc::chart::LineChart c;
        c.set_show_legend(true);
        c.add_series(series("sin", Color::ansi16(2), cat::sine_pts(40, 0.20)));
        c.add_series(series("cos", Color::ansi16(4), cat::sine_pts(40, 0.20 + 1.5708)));
        c.draw({cv});
    });
    demo("笛卡尔", "柱状 Bar", "BarChart", [](tc::canvas::Canvas& cv) {
        tc::chart::BarChart c;
        c.set_values({5, 9, 3, 7, 6, 8}, {"一", "二", "三", "四", "五", "六"}, Color::ansi16(4), "v");
        c.draw({cv});
    });
    demo("笛卡尔", "散点 Scatter", "ScatterChart", [](tc::canvas::Canvas& cv) {
        tc::chart::ScatterChart c;
        tc::chart::Series s;
        s.name = "pts";
        s.color = Color::ansi16(6);
        std::uint64_t r = 99;
        for (int i = 0; i < 40; ++i) s.points.push_back(Point{u01(r) * 10, u01(r) * 10});
        c.add_series(s);
        c.draw({cv});
    });
    demo("笛卡尔", "气泡 Bubble", "BubbleChart", [](tc::canvas::Canvas& cv) {
        tc::chart::BubbleChart c;
        std::vector<tc::chart::BubbleChart::Bubble> bs;
        std::uint64_t r = 7;
        for (int i = 0; i < 12; ++i) bs.push_back({Point{u01(r) * 10, u01(r) * 10}, 1.0 + u01(r) * 4.0});
        c.set_data(bs);
        c.draw({cv});
    });
    demo("笛卡尔", "箱线 BoxPlot", "BoxPlotChart", [](tc::canvas::Canvas& cv) {
        tc::chart::BoxPlotChart c;
        std::vector<tc::chart::BoxPlotChart::BoxStats> g;
        for (int k = 0; k < 4; ++k) {
            const auto s = normals(60, 100 + static_cast<std::uint64_t>(k));
            double mn = s[0];
            double mx = s[0];
            for (double e : s) {
                mn = std::min(mn, e);
                mx = std::max(mx, e);
            }
            tc::chart::BoxPlotChart::BoxStats b;
            b.lo = mn;
            b.q1 = -0.6;
            b.med = 0.0;
            b.q3 = 0.6;
            b.hi = mx;
            g.push_back(b);
        }
        c.set_data(g);
        c.draw({cv});
    });
    demo("笛卡尔", "直方图 Hist", "HistChart", [](tc::canvas::Canvas& cv) {
        tc::chart::HistChart c;
        c.set_values(normals(400), Color::ansi16(5), "n");
        c.set_bins(20);
        c.draw({cv});
    });
    demo("笛卡尔", "热力 Heatmap", "HeatmapChart", [](tc::canvas::Canvas& cv) {
        tc::chart::HeatmapChart c;
        std::vector<Point> pts;
        std::uint64_t r = 5;
        for (int i = 0; i < 300; ++i) pts.push_back(Point{u01(r) * 10, u01(r) * 10});
        c.set_values(pts, tc::color::ColormapKind::Viridis, "h");
        c.draw({cv});
    });
    demo("笛卡尔", "K 线 Candlestick", "CandlestickChart", [](tc::canvas::Canvas& cv) {
        tc::chart::CandlestickChart c;
        const auto pr = walk(40);
        std::vector<tc::chart::CandlestickChart::Ohlc> bars;
        for (std::size_t i = 2; i + 1 < pr.size(); ++i) {
            tc::chart::CandlestickChart::Ohlc o;
            o.open = pr[i - 1];
            o.close = pr[i];
            o.high = std::max(pr[i - 1], pr[i]) + 1.0;
            o.low = std::min(pr[i - 1], pr[i]) - 1.0;
            bars.push_back(o);
        }
        c.set_data(bars);
        c.draw({cv});
    });
    demo("笛卡尔", "面积 Area", "AreaChart", [](tc::canvas::Canvas& cv) {
        tc::chart::AreaChart c;
        std::vector<Point> pts;
        for (int i = 0; i < 40; ++i) pts.push_back(Point{static_cast<double>(i), std::sin(i * 0.2) * 3.0 + 3.0});
        c.set_values(pts, Color::ansi16(6), "a");
        c.draw({cv});
    });
    demo("笛卡尔", "误差棒 ErrorBar", "ErrorBarChart", [](tc::canvas::Canvas& cv) {
        tc::chart::ErrorBarChart c;
        std::vector<tc::chart::ErrorBarChart::ErrorPoint> es;
        for (int i = 0; i < 10; ++i) {
            tc::chart::ErrorBarChart::ErrorPoint e;
            e.x = static_cast<double>(i);
            e.y = std::sin(i * 0.5) + 5.0;
            e.y_lo = e.y - 0.6;
            e.y_hi = e.y + 0.6;
            es.push_back(e);
        }
        c.set_data(es);
        c.draw({cv});
    });
    demo("笛卡尔", "甘特 Gantt", "GanttChart", [](tc::canvas::Canvas& cv) {
        tc::chart::GanttChart c;
        std::vector<tc::chart::GanttChart::Task> ts;
        ts.push_back({"设计", 0.0, 3.0});
        ts.push_back({"开发", 2.0, 7.0});
        ts.push_back({"测试", 6.0, 9.0});
        ts.push_back({"发布", 9.0, 10.0});
        c.set_data(ts);
        c.draw({cv});
    });
    demo("笛卡尔", "等高线 Contour", "ContourChart", [](tc::canvas::Canvas& cv) {
        tc::chart::ContourChart c;
        std::vector<std::vector<double>> f;
        for (int y = 0; y < 16; ++y) {
            std::vector<double> row;
            for (int x = 0; x < 24; ++x) {
                const double dx = x - 12.0;
                const double dy = y - 8.0;
                row.push_back(std::sqrt(dx * dx + dy * dy));
            }
            f.push_back(row);
        }
        c.set_field(f);
        c.draw({cv});
    });
    demo("笛卡尔", "小提琴 Violin", "ViolinChart", [](tc::canvas::Canvas& cv) {
        tc::chart::ViolinChart c;
        std::vector<std::vector<double>> g;
        for (int k = 0; k < 4; ++k) g.push_back(normals(120, 200 + static_cast<std::uint64_t>(k)));
        c.set_data(g);
        c.draw({cv});
    });
    demo("笛卡尔", "峰峦 Ridgeline", "RidgelineChart", [](tc::canvas::Canvas& cv) {
        tc::chart::RidgelineChart c;
        std::vector<tc::chart::RidgelineChart::Group> gs;
        for (int k = 0; k < 4; ++k) {
            tc::chart::RidgelineChart::Group g;
            g.name = std::string("G") + std::to_string(k);
            g.values = normals(150, 300 + static_cast<std::uint64_t>(k));
            g.color = Color::ansi16(static_cast<std::uint8_t>(2 + k));
            gs.push_back(g);
        }
        c.set_groups(gs);
        c.draw({cv});
    });
    demo("笛卡尔", "平行坐标 Parallel", "ParallelChart", [](tc::canvas::Canvas& cv) {
        tc::chart::ParallelChart c;
        std::vector<std::vector<double>> obs;
        std::uint64_t r = 42;
        for (int i = 0; i < 12; ++i) {
            obs.push_back({u01(r) * 10, u01(r) * 10, u01(r) * 10, u01(r) * 10});
        }
        c.set_axes({"a", "b", "c", "d"}, obs);
        c.draw({cv});
    });
    demo("笛卡尔", "Q-Q 图", "QQChart", [](tc::canvas::Canvas& cv) {
        tc::chart::QQChart c;
        c.set_sample(normals(200));
        c.draw({cv});
    });
    demo("笛卡尔", "阶梯 Step", "StepChart", [](tc::canvas::Canvas& cv) {
        tc::chart::StepChart c;
        std::vector<Point> pts;
        for (int i = 0; i < 20; ++i) pts.push_back(Point{static_cast<double>(i), (i % 4 < 2) ? 3.0 : 7.0});
        c.set_values(pts, Color::ansi16(3), "s");
        c.draw({cv});
    });
    demo("笛卡尔", "流图 Streamgraph", "StreamgraphChart", [](tc::canvas::Canvas& cv) {
        tc::chart::StreamgraphChart c;
        std::vector<tc::chart::Series> ss;
        for (int k = 0; k < 3; ++k) {
            tc::chart::Series s;
            s.name = std::string("s") + std::to_string(k);
            s.color = Color::ansi16(static_cast<std::uint8_t>(2 + k * 2));
            for (int i = 0; i < 30; ++i) s.points.push_back(Point{static_cast<double>(i), 1.0 + std::abs(std::sin(i * 0.3 + k))});
            ss.push_back(s);
        }
        c.set_series(ss);
        c.draw({cv});
    });
    demo("笛卡尔", "瀑布 Waterfall", "WaterfallChart", [](tc::canvas::Canvas& cv) {
        tc::chart::WaterfallChart c;
        std::vector<tc::chart::WaterfallChart::Item> it;
        it.push_back({"起", 10, false});
        it.push_back({"增", 5, false});
        it.push_back({"减", -3, false});
        it.push_back({"增", 4, false});
        it.push_back({"终", 0, true});
        c.set_data(it);
        c.draw({cv});
    });
    demo("笛卡尔", "子弹 Bullet", "BulletChart", [](tc::canvas::Canvas& cv) {
        tc::chart::BulletChart c;
        std::vector<tc::chart::BulletChart::Row> rs;
        rs.push_back({"收入", 75, 80, {40, 70, 100}});
        rs.push_back({"利润", 45, 50, {20, 50, 80}});
        c.set_rows(rs);
        c.draw({cv});
    });
    demo("笛卡尔", "哑铃 Dumbbell", "DumbbellChart", [](tc::canvas::Canvas& cv) {
        tc::chart::DumbbellChart c;
        std::vector<tc::chart::DumbbellChart::Row> rs;
        rs.push_back({"A", 2.0, 7.0});
        rs.push_back({"B", 4.0, 5.0});
        rs.push_back({"C", 1.0, 9.0});
        c.set_rows(rs);
        c.draw({cv});
    });
    demo("笛卡尔", "马赛克 Mosaic", "MosaicChart", [](tc::canvas::Canvas& cv) {
        tc::chart::MosaicChart c;
        c.set_counts({{20, 30, 10}, {15, 25, 30}}, {"R1", "R2"}, {"C1", "C2", "C3"});
        c.draw({cv});
    });
    demo("笛卡尔", "迷你线 Sparkline", "SparklineChart", [](tc::canvas::Canvas& cv) {
        tc::chart::SparklineChart c;
        c.set_values(walk(48));
        c.draw({cv});
    });

    // ═══ 极坐标 polar ═══════════════════════════════════════════════════
    demo("极坐标", "饼图 Pie", "PieChart", [](tc::canvas::Canvas& cv) {
        tc::chart::PieChart c;
        c.set_values({30, 25, 20, 15, 10}, {"A", "B", "C", "D", "E"});
        c.draw({cv});
    });
    demo("极坐标", "环形 Donut", "DonutChart", [](tc::canvas::Canvas& cv) {
        tc::chart::DonutChart c;
        c.set_values({40, 30, 20, 10}, {"w", "x", "y", "z"});
        c.draw({cv});
    });
    demo("极坐标", "仪表 Gauge", "GaugeChart", [](tc::canvas::Canvas& cv) {
        tc::chart::GaugeChart c;
        c.set_value(0.72);
        c.draw({cv});
    });
    demo("极坐标", "雷达 Radar", "RadarChart", [](tc::canvas::Canvas& cv) {
        tc::chart::RadarChart c;
        c.set_values({8, 6, 9, 5, 7, 8}, {"力", "敏", "智", "体", "魅", "运"});
        c.draw({cv});
    });
    demo("极坐标", "玫瑰 Rose", "RoseChart", [](tc::canvas::Canvas& cv) {
        tc::chart::RoseChart c;
        c.set_values({5, 8, 3, 9, 6, 4}, {"a", "b", "c", "d", "e", "f"});
        c.draw({cv});
    });
    demo("极坐标", "极轴柱 PolarBar", "PolarBarChart", [](tc::canvas::Canvas& cv) {
        tc::chart::PolarBarChart c;
        c.set_values({3, 7, 5, 9, 6}, {"一", "二", "三", "四", "五"});
        c.draw({cv});
    });
    demo("极坐标", "极轴线 PolarLine", "PolarLineChart", [](tc::canvas::Canvas& cv) {
        tc::chart::PolarLineChart c;
        std::vector<Point> pts;
        for (int i = 0; i <= 60; ++i) {
            const double a = static_cast<double>(i) * 6.0;
            pts.push_back(Point{a, 3.0 + 2.0 * std::sin(a * 0.05)});
        }
        c.set_points(pts);
        c.draw({cv});
    });
    demo("极坐标", "极轴热力 PolarHeat", "PolarHeatChart", [](tc::canvas::Canvas& cv) {
        tc::chart::PolarHeatChart c;
        std::vector<Point> pts;
        std::uint64_t r = 11;
        for (int i = 0; i < 200; ++i) pts.push_back(Point{u01(r) * 360.0, u01(r) * 5.0});
        c.set_points(pts);
        c.draw({cv});
    });

    // ═══ 金融 financial ═════════════════════════════════════════════════
    demo("金融", "深度 Depth", "DepthChart", [](tc::canvas::Canvas& cv) {
        tc::chart::DepthChart c;
        std::vector<double> bp;
        std::vector<double> bq;
        std::vector<double> ap;
        std::vector<double> aq;
        for (int i = 0; i < 8; ++i) {
            bp.push_back(100.0 - i);
            bq.push_back(10.0 + i * 3);
            ap.push_back(101.0 + i);
            aq.push_back(12.0 + i * 2);
        }
        c.set_book(bp, bq, ap, aq);
        c.draw({cv});
    });
    demo("金融", "回撤 Drawdown", "DrawdownChart", [](tc::canvas::Canvas& cv) {
        tc::chart::DrawdownChart c;
        c.set_prices(walk(60));
        c.draw({cv});
    });
    demo("金融", "宝塔 Kagi", "KagiChart", [](tc::canvas::Canvas& cv) {
        tc::chart::KagiChart c;
        c.set_prices(walk(50));
        c.draw({cv});
    });
    demo("金融", "点数 PointFigure", "PointFigureChart", [](tc::canvas::Canvas& cv) {
        tc::chart::PointFigureChart c;
        c.set_prices(walk(60));
        c.draw({cv});
    });
    demo("金融", "砖形 Renko", "RenkoChart", [](tc::canvas::Canvas& cv) {
        tc::chart::RenkoChart c;
        c.set_prices(walk(70));
        c.draw({cv});
    });
    demo("金融", "成交量 Volume", "VolumeChart", [](tc::canvas::Canvas& cv) {
        tc::chart::VolumeChart c;
        const auto pr = walk(40);
        std::vector<int> dirs;
        for (std::size_t i = 1; i < pr.size(); ++i) dirs.push_back(pr[i] >= pr[i - 1] ? 1 : -1);
        std::vector<double> vols;
        std::uint64_t r = 3;
        for (std::size_t i = 1; i < pr.size(); ++i) vols.push_back(5.0 + u01(r) * 20.0);
        c.set_values(vols, dirs);
        c.draw({cv});
    });
    demo("金融", "收益率曲线", "YieldCurveChart", [](tc::canvas::Canvas& cv) {
        tc::chart::YieldCurveChart c;
        c.set_curve({0.25, 0.5, 1, 2, 3, 5, 7, 10}, {1.5, 1.8, 2.2, 2.6, 2.9, 3.1, 3.2, 3.15});
        c.draw({cv});
    });

    // ═══ 统计 statistical ════════════════════════════════════════════════
    demo("统计", "自相关 Autocorr", "AutocorrChart", [](tc::canvas::Canvas& cv) {
        tc::chart::AutocorrChart c;
        c.set_values(walk(80));
        c.draw({cv});
    });
    demo("统计", "Bland-Altman", "BlandAltmanChart", [](tc::canvas::Canvas& cv) {
        tc::chart::BlandAltmanChart c;
        std::vector<double> a;
        std::vector<double> b;
        const auto n = normals(60, 501);
        for (std::size_t i = 0; i < n.size(); ++i) {
            a.push_back(10.0 + n[i]);
            b.push_back(10.2 + n[i] * 0.9 + 0.3);
        }
        c.set_pairs(a, b, Color::ansi16(5), "m");
        c.draw({cv});
    });
    demo("统计", "相关矩阵 CorrMatrix", "CorrMatrixChart", [](tc::canvas::Canvas& cv) {
        tc::chart::CorrMatrixChart c;
        std::vector<std::vector<double>> cols;
        for (int k = 0; k < 4; ++k) cols.push_back(normals(50, 600 + static_cast<std::uint64_t>(k)));
        c.set_matrix({"A", "B", "C", "D"}, cols);
        c.draw({cv});
    });
    demo("统计", "分布 KDE", "DistributionChart", [](tc::canvas::Canvas& cv) {
        tc::chart::DistributionChart c;
        c.set_values(normals(300), Color::ansi16(6), "kde");
        c.draw({cv});
    });
    demo("统计", "经验分布 ECDF", "EcdfChart", [](tc::canvas::Canvas& cv) {
        tc::chart::EcdfChart c;
        c.set_values(normals(200), Color::ansi16(4), "e");
        c.draw({cv});
    });
    demo("统计", "森林图 Forest", "ForestChart", [](tc::canvas::Canvas& cv) {
        tc::chart::ForestChart c;
        std::vector<tc::chart::ForestStudy> st;
        st.push_back({"研究A", 0.5, 0.2, 0.8});
        st.push_back({"研究B", -0.3, -0.7, 0.1});
        st.push_back({"研究C", 0.8, 0.4, 1.2});
        c.set_studies(st);
        c.draw({cv});
    });
    demo("统计", "主成分 PCA", "PcaChart", [](tc::canvas::Canvas& cv) {
        tc::chart::PcaChart c;
        std::vector<std::vector<double>> rows;
        std::uint64_t r = 71;
        for (int i = 0; i < 20; ++i) rows.push_back({u01(r) * 10, u01(r) * 10, u01(r) * 10});
        c.set_data(rows);
        c.draw({cv});
    });
    demo("统计", "回归 Regression", "RegressionChart", [](tc::canvas::Canvas& cv) {
        tc::chart::RegressionChart c;
        std::vector<Point> pts;
        std::uint64_t r = 17;
        for (int i = 0; i < 40; ++i) {
            const double x = u01(r) * 10;
            pts.push_back(Point{x, 2.0 * x + 1.0 + (u01(r) - 0.5) * 3.0});
        }
        c.set_points(pts, Color::ansi16(2), "fit");
        c.draw({cv});
    });
    demo("统计", "ROC 曲线", "RocChart", [](tc::canvas::Canvas& cv) {
        tc::chart::RocChart c;
        std::vector<double> scores;
        std::vector<int> labels;
        std::uint64_t r = 23;
        for (int i = 0; i < 60; ++i) {
            scores.push_back(u01(r) * 10);
            labels.push_back(u01(r) < 0.5 ? 0 : 1);
        }
        c.set_scores(scores, labels, "roc");
        c.draw({cv});
    });
    demo("统计", "季节分解 Seasonal", "SeasonalChart", [](tc::canvas::Canvas& cv) {
        tc::chart::SeasonalChart c;
        std::vector<double> v;
        for (int i = 0; i < 60; ++i) v.push_back(10.0 + i * 0.2 + 3.0 * std::sin(i * 0.6));
        c.set_values(v, Color::ansi16(3));
        c.draw({cv});
    });
    demo("统计", "生存 Survival", "SurvivalChart", [](tc::canvas::Canvas& cv) {
        tc::chart::SurvivalChart c;
        std::vector<double> times;
        std::vector<int> events;
        std::uint64_t r = 29;
        for (int i = 0; i < 40; ++i) {
            times.push_back(u01(r) * 24);
            events.push_back(u01(r) < 0.7 ? 1 : 0);
        }
        c.set_data(times, events, Color::ansi16(2), "s");
        c.draw({cv});
    });

    // ═══ 层级 hierarchical ══════════════════════════════════════════════
    auto hier_root = [] {
        return tc::chart::hier::HierNode{
            "根", 0, Color{}, {
                {"甲", 30, Color::ansi16(2), {}},
                {"乙", 20, Color::ansi16(3), {{"乙1", 12, Color::ansi16(4), {}}, {"乙2", 8, Color::ansi16(5), {}}}},
                {"丙", 25, Color::ansi16(6), {}},
            }};
    };
    demo("层级", "矩形树图 Treemap", "Treemap", [hier_root](tc::canvas::Canvas& cv) {
        tc::chart::Treemap c;
        c.set_root(hier_root());
        c.draw({cv});
    });
    demo("层级", "打包图 Pack", "PackChart", [hier_root](tc::canvas::Canvas& cv) {
        tc::chart::PackChart c;
        c.set_root(hier_root());
        c.draw({cv});
    });
    demo("层级", "旭日 Sunburst", "Sunburst", [hier_root](tc::canvas::Canvas& cv) {
        tc::chart::Sunburst c;
        c.set_root(hier_root());
        c.draw({cv});
    });
    demo("层级", "冰柱 Icicle", "Icicle", [hier_root](tc::canvas::Canvas& cv) {
        tc::chart::Icicle c;
        c.set_root(hier_root());
        c.draw({cv});
    });
    demo("层级", "系统树 Dendrogram", "Dendrogram", [hier_root](tc::canvas::Canvas& cv) {
        tc::chart::Dendrogram c;
        c.set_root(hier_root());
        c.draw({cv});
    });
    demo("层级", "树视图 TreeView", "TreeView", [hier_root](tc::canvas::Canvas& cv) {
        tc::chart::TreeView c;
        c.set_root(hier_root());
        c.draw({cv});
    });

    // ═══ 地理 geo ═══════════════════════════════════════════════════════
    demo("地理", "六边形 Hexbin", "Hexbin", [](tc::canvas::Canvas& cv) {
        tc::chart::Hexbin c;
        std::uint64_t r = 13;
        for (int i = 0; i < 200; ++i) c.add_point(u01(r) * 20, u01(r) * 12);
        c.draw({cv});
    });
    demo("地理", "等值区域 Choropleth", "Choropleth", [](tc::canvas::Canvas& cv) {
        tc::chart::Choropleth c;
        c.add_region({"北", {Point{2, 2}, Point{10, 2}, Point{10, 8}, Point{2, 8}}, 0.8});
        c.add_region({"南", {Point{12, 2}, Point{22, 2}, Point{22, 8}, Point{12, 8}}, 0.3});
        c.draw({cv});
    });
    demo("地理", "点密度 DotDensity", "DotDensity", [](tc::canvas::Canvas& cv) {
        tc::chart::DotDensity c;
        c.add_region({"区", {Point{3, 3}, Point{18, 3}, Point{18, 12}, Point{3, 12}}, 40, Color::ansi16(4)});
        c.draw({cv});
    });
    demo("地理", "瓦片网格 TileGrid", "TileGrid", [](tc::canvas::Canvas& cv) {
        tc::chart::TileGrid c;
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 6; ++col) {
                c.add_tile({std::string("t") + std::to_string(row * 6 + col), col, row,
                            (row * 6 + col) / 18.0});
            }
        }
        c.draw({cv});
    });

    // ═══ 指示器 indicator ═══════════════════════════════════════════════
    demo("指示器", "KPI", "GaugeKpi", [](tc::canvas::Canvas& cv) {
        tc::chart::GaugeKpi c;
        c.set_label("营收");
        c.set_value("¥ 1.28M");
        c.set_delta("+12.4%", true);
        c.draw({cv});
    });
    demo("指示器", "进度环", "ProgressRing", [](tc::canvas::Canvas& cv) {
        tc::chart::ProgressRing c;
        c.set_fraction(0.68);
        c.draw({cv});
    });
    demo("指示器", "星级", "StarRating", [](tc::canvas::Canvas& cv) {
        tc::chart::StarRating c;
        c.set_rating(3.6);
        c.draw({cv});
    });
    demo("指示器", "状态灯", "StatusLight", [](tc::canvas::Canvas& cv) {
        tc::chart::StatusLight c;
        c.set_state(tc::chart::LightState::Ok);
        c.set_label("正常");
        c.draw({cv});
    });
    demo("指示器", "温度计", "Thermometer", [](tc::canvas::Canvas& cv) {
        tc::chart::Thermometer c;
        c.set_value(0.62);
        c.draw({cv});
    });

    // ═══ 文本 text ══════════════════════════════════════════════════════
    demo("文本", "词云 Wordcloud", "Wordcloud", [](tc::canvas::Canvas& cv) {
        tc::chart::Wordcloud c;
        c.add_word("termchart", 10);
        c.add_word("终端", 8);
        c.add_word("图表", 7);
        c.add_word("C++20", 6);
        c.add_word("零依赖", 5);
        c.add_word("渲染", 4);
        c.draw({cv});
    });
    demo("文本", "标签云 TagCloud", "TagCloud", [](tc::canvas::Canvas& cv) {
        tc::chart::TagCloud c;
        c.add_tag("cpp", 9);
        c.add_tag("chart", 6);
        c.add_tag("tui", 4);
        c.add_tag("gui", 3);
        c.draw({cv});
    });
    demo("文本", "进度条 Progress", "Progress", [](tc::canvas::Canvas& cv) {
        tc::chart::Progress c;
        c.add_bar("下载", 0.8, Color::ansi16(2));
        c.add_bar("解压", 0.5, Color::ansi16(3));
        c.add_bar("安装", 0.25, Color::ansi16(4));
        c.draw({cv});
    });
    demo("文本", "表格 Table", "Table", [](tc::canvas::Canvas& cv) {
        tc::chart::Table c;
        c.add_column("名称");
        c.add_column("值");
        c.add_row({"甲", "1"});
        c.add_row({"乙", "2"});
        c.add_row({"丙", "3"});
        c.draw({cv});
    });

    // ═══ 流向 flow ══════════════════════════════════════════════════════
    demo("流向", "漏斗 Funnel", "FunnelChart", [](tc::canvas::Canvas& cv) {
        tc::chart::FunnelChart c;
        std::vector<tc::chart::FunnelStage> st;
        st.push_back({"访问", 1000, Color::ansi16(4)});
        st.push_back({"注册", 600, Color::ansi16(5)});
        st.push_back({"付费", 200, Color::ansi16(6)});
        c.set_stages(st);
        c.draw({cv});
    });
    demo("流向", "河流图 Stream", "StreamChart", [](tc::canvas::Canvas& cv) {
        tc::chart::StreamChart c;
        std::vector<tc::chart::Series> ss;
        for (int k = 0; k < 3; ++k) {
            tc::chart::Series s;
            s.name = std::string("f") + std::to_string(k);
            s.color = Color::ansi16(static_cast<std::uint8_t>(2 + k));
            for (int i = 0; i < 24; ++i) s.points.push_back(Point{static_cast<double>(i), 1.0 + std::abs(std::cos(i * 0.4 + k))});
            ss.push_back(s);
        }
        c.set_series(ss);
        c.draw({cv});
    });

    // ═══ 色彩 colormap / palette ════════════════════════════════════════
    demo("色彩", "5 色带", "Colormaps", [](tc::canvas::Canvas& cv) {
        const tc::color::ColormapKind kinds[] = {tc::color::ColormapKind::Viridis,
                                                 tc::color::ColormapKind::Inferno,
                                                 tc::color::ColormapKind::Magma,
                                                 tc::color::ColormapKind::Plasma,
                                                 tc::color::ColormapKind::Turbo};
        int row = 0;
        for (auto k : kinds) {
            for (int x = 0; x < 34; ++x) {
                const double t = static_cast<double>(x) / 33.0;
                const tc::Rgb c = tc::color::sample(k, t);
                static_cast<void>(cv.put(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(row),
                                         U' ', Style{}.set_bg(Color::rgb(c))));
            }
            row += 2;
        }
    });
    demo("色彩", "命名色查找", "named_colors", [](tc::canvas::Canvas& cv) {
        int x = 0;
        int y = 0;
        for (const char* nm : {"tomato", "gold", "skyblue", "orchid", "seagreen", "crimson"}) {
            const auto c = tc::color::lookup_named_color(nm);
            if (c) {
                for (int i = 0; i < 5; ++i) {
                    static_cast<void>(cv.put(static_cast<std::uint32_t>(x + i),
                                             static_cast<std::uint32_t>(y), U' ',
                                             Style{}.set_bg(Color::rgb(*c))));
                }
            }
            x += 6;
            if (x > 28) {
                x = 0;
                ++y;
            }
        }
    });

    // ═══ math 特殊函数曲线 ══════════════════════════════════════════════
    demo("数学", "特殊函数", "math::special", [](tc::canvas::Canvas& cv) {
        tc::chart::LineChart c;
        c.set_show_legend(true);
        std::vector<Point> erf;
        std::vector<Point> j0;
        for (int i = 0; i <= 40; ++i) {
            const double x = -3.0 + 6.0 * i / 40.0;
            erf.push_back(Point{x, tc::math::erf(x)});
            j0.push_back(Point{x, tc::math::bessel_j0(x)});
        }
        c.add_series(series("erf", Color::ansi16(4), erf));
        c.add_series(series("J0", Color::ansi16(6), j0));
        c.draw({cv});
    });
}

}  // namespace

void register_charts(std::vector<Demo>& d) { register_charts_impl(d); }

}  // namespace cat
